#!/usr/bin/env bash
# Makes a player account, or a GM account with "gm", run as root on the server from the repository folder:
#   bash modules/mod-titan/account.sh        a player account (for friends)
#   bash modules/mod-titan/account.sh gm     a GM account
# Running it again with an existing name sets that account's password (a forgotten one); a player run never
# changes an account's GM level. In game, .account create needs the console, which this stands in for.
set -u

db() { docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -N acore_auth 2>/dev/null'; }
gm=0
[ "${1:-}" = "gm" ] && gm=1
kind=$([ $gm = 1 ] && echo "GM account" || echo "Player account")

command -v python3 > /dev/null || apt-get install -y -qq python3 > /dev/null
read -rp "$kind name (letters and numbers): " name
read -rsp "$kind password (nothing shows while you type, then press Enter): " pass
echo
read -rsp "Type the password again: " again
echo
if [ "$pass" != "$again" ]; then
    echo "FAILED: the two passwords differ. Run it again."
    exit 1
fi
name=$(echo "$name" | tr -d '[:space:]' | tr '[:lower:]' '[:upper:]')
if ! [[ "$name" =~ ^[A-Z0-9]{3,16}$ ]]; then
    echo "FAILED: the name \"$name\" must be 3-16 letters or numbers, no spaces or symbols. Run it again."
    exit 1
fi
if [ ${#pass} -lt 1 ] || [ ${#pass} -gt 16 ]; then
    echo "FAILED: the password was ${#pass} characters; the game allows 1-16. Run it again."
    exit 1
fi
# A new account, or a new password for one that exists (running this again resets a forgotten password).
# The SRP6 registration data the authserver checks (SRP6::MakeRegistrationData).
keys=$(NAME="$name" PASS="$pass" python3 -c '
import hashlib, os
N = int("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)
salt = os.urandom(32)
h1 = hashlib.sha1((os.environ["NAME"] + ":" + os.environ["PASS"].upper()).encode()).digest()
x = int.from_bytes(hashlib.sha1(salt + h1).digest(), "little")
print("X'"'"'%s'"'"', X'"'"'%s'"'"'" % (salt.hex(), pow(7, x, N).to_bytes(32, "little").hex()))
')
salt=${keys%%,*}
verifier=${keys#*, }
echo "INSERT INTO account (username, salt, verifier, expansion) VALUES ('$name', $salt, $verifier, 2)
    ON DUPLICATE KEY UPDATE salt=$salt, verifier=$verifier;" | db
if [ $gm = 1 ]; then
    echo "INSERT INTO account_access (id, gmlevel, RealmID, comment) SELECT id, 3, -1, 'account.sh' FROM account
        WHERE username='$name' ON DUPLICATE KEY UPDATE gmlevel=3;" | db
    check="SELECT id FROM account_access WHERE gmlevel=3 AND id=(SELECT id FROM account WHERE username='$name');"
else
    check="SELECT id FROM account WHERE username='$name';"
fi
if [ -n "$(echo "$check" | db)" ]; then
    echo "OK: $name is a ${kind,,}, with the password you just typed."
else
    echo "FAILED: could not make the account."
    exit 1
fi
