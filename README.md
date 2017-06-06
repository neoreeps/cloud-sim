# cloud-sim
Amazon S3 cloud simulator running on Apache


# CSIM Apache Config

## Server
* Ubuntu Server 14.04 LTS- Ubuntu is one of the most common distros which many are already familiar. Using the Server version since X is not required for the sim and just gets in the way, also reduces resource requirements. This is an LTS release which means updates for at least 4 years.

* Network based install
    * OpenSSH Server
    * LAMP - Linux, Apache, MySql, PHP
* edit /etc/network/interfaces
```
upport@newsim:/etc/network$ cat interfaces
# This file describes the network interfaces available on your system
# and how to activate them. For more information, see interfaces(5).

# The loopback network interface
auto lo
iface lo inet loopback

# The primary network interface
auto eth0
iface eth0 inet static
address 192.168.56.38
netmask 255.255.255.0
network 192.168.56.0
broadcast 192.168.56.255
gateway 192.168.56.2
dns-nameservers 192.168.56.254
```

## Apache2
NOTE: Install the LAMP stack which should provide all the dependencies you need
sudo apt-get install lamp-server^
Don't skip the ^ since this is a dummy package.

Enable SSL
* a2enmod ssl
Which essentially performs but is much simpler:
* ln -s /etc/apache2/sites-available/default-ssl.conf /etc/apache2/sites-enabled/
* ln -s /etc/apache2/mods-available/ssl* /etc/apache2/mods-enabled/
* ln -s /etc/apache2/mods-available/socache_shmcb.load /etc/apache2/mods-enabled/
Create certificate
* make-ssl-cert generate-default-snakeoil --force-overwrite

Setup MPM
Edit /etc/apache2/mods-available/actions.conf and add the line:
Action application/x-httpd-php /cgi-bin/php5
Install php5-cgi
* apt-get install php5-cgi
* a2enmod cgid

Miscellaneous
Enable cURL required for Google
* Not sure if this is needed since gAPI is b0rk
* TODO - Fix PUT apache 500 error
* apt-get install php5-curl libapache2-mod-php5
Disable Compression (deflate) Since we aren't running a real web-server, disable the 'deflate' mod to bypass the mime type rule checking. We shouldn't match any of the rules anyway.
* a2dismod deflate
Enable Server Status
* a2enmod status
Update the conf to allow access from any host:
* Edit mods-available/status.conf
* Edit the Location block as follows:
```
        <Location /server-status>
                SetHandler server-status
                Allow from all
                #Require local
                #Require ip 192.0.2.0/24
        </Location>
```
* restart apache
You can now access the server status via: http://{serverName}/server-status

Virtual Hosts and Rewrite
We need to enable rewrite for GET methods else Apache tries to open/execute the files.
* a2enmod rewrite
* a2enmod actions
Create new virtual hosts based on the default and default-ssl configs:
* cd /etc/apache2/sites-available
* cp 000-default.conf csim.conf
* cp default-ssl.conf csim-ssl.conf
Copy the csim binary to /var/www/cgi-bin
Now, edit csim.conf and csim-ssl.conf:
* Add the following at the beginning of the file below the <VirtualHost *> tag.
```
        ServerAdmin webmaster@localhost

        DocumentRoot /var/www/html
        <Directory /var/www/html>
            Options Indexes FollowsymLinks MultiViews ExecCGI
            AllowOverride None
            Order allow,deny
            Allow from all

            RewriteEngine On
            RewriteCond %{REQUEST_METHOD} GET
            RewriteRule ^(.*)$ $?dummy [L,NS]

            Script POST /cgi-bin/csim
            Script PUT /cgi-bin/csim
            Script GET /cgi-bin/csim
            #Script HEAD /cgi-bin/csim
            Script DELETE /cgi-bin/csim
        </Directory>

        ScriptAlias /cgi-bin/ /var/www/cgi-bin/
        <Directory "/var/www/cgi-bin">
            AllowOverride None
            Options +ExecCGI -MultiViews +SymLinksIfOwnerMatch
            Order allow,deny
            Allow from all
        </Directory>

        # Available loglevels: trace8, ..., trace1, debug, info, notice, warn,
        # error, crit, alert, emerg.
        # It is also possible to configure the loglevel for particular
        # modules, e.g.
        LogLevel info ssl:warn rewrite:info 

        # Dont log normal access, saves a lot of IO on a cloudsim
        ErrorLog ${APACHE_LOG_DIR}/error_csim.log
        SetEnvIf Request_URI "\.(snp|jpg|xml|png|gif|ico|js|css|swf|js?.|css?.)$" DontLog
        CustomLog ${APACHE_LOG_DIR}/access_csim.log combined Env=!DontLog
```

* Finally, enable the new sites:
* a2ensite csim csim-ssl

Directory Structure
* /var/www/html is the document root and contains all of the data.
* The expectation is that each application will use a unique "bucket" and "path" which will keep their data separate.
    * amazon - /var/www/html/{bucket}/{path}/
    * atmos - /var/www/html/rest/namespace/{path}/

* Copy the simulator binary files to /var/www/cgi-bin/
* Restart apache with /etc/init.d/apache2 restart

