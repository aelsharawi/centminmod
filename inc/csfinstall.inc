csfinstalls() {

    # centos 6.6 minimal bug
    # https://bugzilla.redhat.com/show_bug.cgi?id=1161682
    # if [[ "$CENTOS_SIX" = '6' && ! -f /etc/sysconfig/iptables ]]; then
    #     yum -y install system-config-firewall-base
    #     lokkit --default=server
    #     service iptables restart
    # fi

# CSF is compulsory
CSFINSTALLOK='y'

#ASK "Install CSF firewall script ? [y/n] "
if [[ "$CSFINSTALLOK" = [yY] ]]; then

    echo "*************************************************"
    cecho "* Installing CSF firewall... " $boldgreen
    echo "*************************************************"
    echo "Installing..."

    cd $DIR_TMP

#download csf tarball

    if [[ $(rpm -q perl-Crypt-SSLeay >/dev/null 2>&1; echo $?) != '0' ]] || [[ $(rpm -q perl-Net-SSLeay >/dev/null 2>&1; echo $?) != '0' ]]; then
        yum${CACHESKIP} -y install perl-libwww-perl perl-Crypt-SSLeay perl-Net-SSLeay
        sar_call
    elif [[ -z "$(rpm -qa perl-libwww-perl)" ]]; then
        yum${CACHESKIP} -y install perl-libwww-perl
        sar_call
    fi
    if [[ "$CENTOS_SEVEN" = '7' ]]; then
        if [[ $(rpm -q perl-LWP-Protocol-https >/dev/null 2>&1; echo $?) != '0' ]]; then
            yum${CACHESKIP} -y install perl-LWP-Protocol-https
            sar_call
        fi
    fi

    #tar xzf csf.tgz
    cd "$DIR_TMP/csf"
    sh install.sh
    sar_call

    # echo "Test IP Tables Modules..."

    # perl /etc/csf/csftest.pl
    cp -a /etc/csf/csf.conf /etc/csf/csf.conf-bak

    echo "CSF adding memcached, varnish ports to csf.allow list..."
    sed -i 's/20,21,22,25,53,80,110,143,443,465,587,993,995/20,21,22,25,53,80,110,111,143,161,443,465,587,993,995,1110,1186,1194,2049,81,9418,30001:50011/g' /etc/csf/csf.conf

sed -i "s/TCP_OUT = \"/TCP_OUT = \"993,995,465,587,111,2049,1110,1194,9418,/g" /etc/csf/csf.conf
sed -i "s/TCP6_OUT = \"/TCP6_OUT = \"993,995,465,587,/g" /etc/csf/csf.conf
sed -i "s/UDP_IN = \"/UDP_IN = \"67,68,111,2049,1110,33434:33534,/g" /etc/csf/csf.conf
sed -i "s/UDP_OUT = \"/UDP_OUT = \"67,68,111,2049,1110,33434:33534,/g" /etc/csf/csf.conf
sed -i "s/DROP_NOLOG = \"67,68,/DROP_NOLOG = \"/g" /etc/csf/csf.conf

    egrep '^UDP_|^TCP_|^DROP_NOLOG' /etc/csf/csf.conf
    
    echo "Disabling CSF Testing mode (activates firewall)..."
    sed -i 's/TESTING = "1"/TESTING = "0"/g' /etc/csf/csf.conf

csftweaks

#######################################################
# check to see if csf.pignore already has custom apps added

CSFPIGNORECHECK=`grep -E '(user:nginx|user:nsd|exe:/usr/local/bin/memcached)' /etc/csf/csf.pignore`

if [[ -z $CSFPIGNORECHECK ]]; then

    echo "Adding Applications/Users to CSF ignore list..."
cat >>/etc/csf/csf.pignore<<EOF
pexe:/usr/local/lsws/bin/lshttpd.*
pexe:/usr/local/lsws/fcgi-bin/lsphp.*
exe:/usr/local/bin/memcached
cmd:/usr/local/bin/memcached
user:mysql
exe:/usr/sbin/mysqld 
cmd:/usr/sbin/mysqld
user:varnish
exe:/usr/sbin/varnishd
cmd:/usr/sbin/varnishd
exe:/sbin/portmap
cmd:portmap
exe:/usr/libexec/gdmgreeter
cmd:/usr/libexec/gdmgreeter
exe:/usr/sbin/avahi-daemon
cmd:avahi-daemon
exe:/sbin/rpc.statd
cmd:rpc.statd
exe:/usr/libexec/hald-addon-acpi
cmd:hald-addon-acpi
user:nsd
user:nginx
user:ntp
user:dbus
user:smmsp
user:postfix
user:dovecot
user:www-data
user:spamfilter
exe:/usr/libexec/dovecot/imap
exe:/usr/libexec/dovecot/pop3
exe:/usr/libexec/dovecot/anvil
exe:/usr/libexec/dovecot/auth
exe:/usr/libexec/dovecot/pop3-login
exe:/usr/libexec/dovecot/imap-login
exe:/usr/libexec/postfix
exe:/usr/libexec/postfix/bounce
exe:/usr/libexec/postfix/discard
exe:/usr/libexec/postfix/error
exe:/usr/libexec/postfix/flush
exe:/usr/libexec/postfix/local
exe:/usr/libexec/postfix/smtp
exe:/usr/libexec/postfix/smtpd
exe:/usr/libexec/postfix/pickup
exe:/usr/libexec/postfix/tlsmgr
exe:/usr/libexec/postfix/qmgr
exe:/usr/libexec/postfix/virtual
exe:/usr/libexec/postfix/proxymap
exe:/usr/libexec/postfix/anvil
exe:/usr/libexec/postfix/lmtp
exe:/usr/libexec/postfix/scache
exe:/usr/libexec/postfix/cleanup
exe:/usr/libexec/postfix/trivial-rewrite
exe:/usr/libexec/postfix/master
EOF

fi # check to see if csf.pignore already has custom apps added

    csf -u
    cmchkconfig csf on
    cmservice csf restart
    csf -r

    cmchkconfig lfd on
    cmservice lfd start

# if CentOS 7 is detected disable firewalld in favour 
# of csf iptables ip6tables for now
if [[ "$CENTOS_SEVEN" = '7' ]]; then
    if [[ "$FIREWALLD_DISABLE" = [yY] ]]; then
        # disable firewalld
        systemctl disable firewalld
        systemctl stop firewalld
    
        # install iptables-services package
        if [ ! -f /usr/lib/systemd/system/iptables.service ]; then
            yum -y install iptables-services
            sar_call
        fi
    
        # start iptables and ip6tables services
        systemctl start iptables
        systemctl start ip6tables
        systemctl enable iptables
        systemctl enable ip6tables
    else
        # leave firewalld enabled
        # disable CSF firewall instead
        service csf stop
        service lfd stop
        chkconfig csf off
        chkconfig lfd off

        # as CSF Firewall is disabled
        # need to setup firewalld permanent
        # services for default public zone
        firewall-cmd --permanent --zone=public --add-service=dns
        firewall-cmd --permanent --zone=public --add-service=ftp
        firewall-cmd --permanent --zone=public --add-service=http
        firewall-cmd --permanent --zone=public --add-service=https
        firewall-cmd --permanent --zone=public --add-service=imaps
        firewall-cmd --permanent --zone=public --add-service=mysql
        firewall-cmd --permanent --zone=public --add-service=pop3s
        firewall-cmd --permanent --zone=public --add-service=smtp
        firewall-cmd --permanent --zone=public --add-service=openvpn
        # firewall-cmd --permanent --zone=public --add-service=nfs

        # firewall-cmd --reload
        systemctl restart firewalld
        firewall-cmd --zone=public --list-services

        # custom ports allowed if detected SSHD default port is not 22, ensure the custom SSHD port
        # number is whitelisted by firewalld
        FWDDETECTED_PORT=$(awk '/^Port / {print $2}' /etc/ssh/sshd_config)
        if [[ "$FWDDETECTED_PORT" = '22' ]]; then
          FIREWALLD_PORTS='8080 8888 81 9312 9418 30001-50011'
        else
          FIREWALLD_PORTS="$FWDDETECTED_PORT 8080 8888 81 9312 9418 30001-50011"
        fi

        for fp in $FIREWALLD_PORTS
          do
            firewall-cmd --permanent --zone=public --add-port=${fp}/tcp
        done

        firewall-cmd --reload
        firewall-cmd --zone=public --list-ports
    fi
fi

    if [ -f /usr/local/src/centminmod/tools/csfcf.sh ]; then
        /usr/local/src/centminmod/tools/csfcf.sh auto
    fi

    echo "*************************************************"
    cecho "* CSF firewall installed " $boldgreen
    echo "*************************************************"
fi

}