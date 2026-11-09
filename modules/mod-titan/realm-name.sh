#!/usr/bin/env bash
# Renames the realm, run as root on the server from the repository folder:
#   bash modules/mod-titan/realm-name.sh Emberforge
# The realm list shows the new name within a minute (the authserver re-reads realmlist), or at once after
# docker restart ac-authserver.
set -u

name="${*:-Emberforge}"
if ! [[ "$name" =~ ^[A-Za-z0-9\ \'-]{1,32}$ ]]; then
    echo "FAILED: use letters, numbers, spaces, ' or - (up to 32 characters)."
    exit 1
fi
db() { docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -N acore_auth 2>/dev/null'; }
echo "UPDATE realmlist SET name='${name//\'/\'\'}' WHERE id=1;" | db
echo "OK: the realm is now called $(echo "SELECT name FROM realmlist WHERE id=1;" | db)"
