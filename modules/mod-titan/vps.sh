#!/usr/bin/env bash
# Emberforge VPS setup, run as root on the server from the repository folder: bash modules/mod-titan/vps.sh
# Sets up nightly backups, points the realm at this server's public IP and makes a GM account. The database
# password is read inside the database container, so nothing here needs typing but the GM account's name and
# password. Safe to run again.
set -u

db() { docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -N acore_auth 2>/dev/null'; }

echo
echo "== 1/3 Backups"
mkdir -p /root/backups
cat > /root/backup.sh <<'EOF'
#!/bin/sh
docker exec ac-database sh -c 'mysqldump -uroot -p"$MYSQL_ROOT_PASSWORD" --databases acore_auth acore_characters \
    2>/dev/null' | gzip > /root/backups/db-$(date +%F).sql.gz
find /root/backups -name 'db-*.sql.gz' -mtime +14 -delete
EOF
echo '30 4 * * * root sh /root/backup.sh' > /etc/cron.d/emberforge-backup
sh /root/backup.sh
latest=$(ls -t /root/backups/db-*.sql.gz | head -1)
if [ "$(stat -c %s "$latest")" -gt 10000 ]; then
    echo "OK: $latest ($(du -h "$latest" | cut -f1)). A new one is made every night at 4:30, kept 14 days."
else
    echo "FAILED: the backup is empty. Is the database running? (docker ps)"
fi

echo
echo "== 2/3 Realm address"
ip=$(curl -s -4 ifconfig.me)
if [ -z "$ip" ]; then
    echo "FAILED: could not find this server's public IP."
else
    echo "UPDATE realmlist SET address='$ip' WHERE id=1;" | db
    echo "OK: the realm now points at $(echo "SELECT CONCAT(name, ' -> ', address) FROM realmlist WHERE id=1;" | db)"
fi

echo
echo "== 3/3 GM account"
command -v python3 > /dev/null || apt-get install -y -qq python3 > /dev/null
read -rp "GM account name (letters and numbers): " name
read -rsp "GM account password (typing stays hidden): " pass
echo
name=$(echo "$name" | tr '[:lower:]' '[:upper:]')
if ! [[ "$name" =~ ^[A-Z0-9]{3,16}$ ]] || [ ${#pass} -lt 1 ] || [ ${#pass} -gt 16 ]; then
    echo "FAILED: the name must be 3-16 letters or numbers and the password 1-16 characters. Run the script again."
    exit 1
fi
if [ -z "$(echo "SELECT id FROM account WHERE username='$name';" | db)" ]; then
    # The SRP6 registration data the authserver checks (SRP6::MakeRegistrationData).
    keys=$(NAME="$name" PASS="$pass" python3 -c '
import hashlib, os
N = int("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)
salt = os.urandom(32)
h1 = hashlib.sha1((os.environ["NAME"] + ":" + os.environ["PASS"].upper()).encode()).digest()
x = int.from_bytes(hashlib.sha1(salt + h1).digest(), "little")
print("X'"'"'%s'"'"', X'"'"'%s'"'"'" % (salt.hex(), pow(7, x, N).to_bytes(32, "little").hex()))
')
    echo "INSERT INTO account (username, salt, verifier, expansion) VALUES ('$name', $keys, 2);" | db
fi
echo "INSERT INTO account_access (id, gmlevel, RealmID, comment) SELECT id, 3, -1, 'vps.sh' FROM account
    WHERE username='$name' ON DUPLICATE KEY UPDATE gmlevel=3;" | db
if [ -n "$(echo "SELECT id FROM account_access WHERE gmlevel=3 AND id=(SELECT id FROM account WHERE username='$name');" \
    | db)" ]; then
    echo "OK: $name is a GM account. If it already existed, its password was left as it was."
else
    echo "FAILED: could not make the account."
fi

echo
echo "All done. On your PC, set realmlist.wtf to: set realmlist $ip"
