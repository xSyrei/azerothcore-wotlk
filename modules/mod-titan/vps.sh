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
bash "$(dirname "$0")/account.sh" gm || exit 1

echo
echo "All done. On your PC, set realmlist.wtf to: set realmlist $ip"
