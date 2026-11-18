#!/usr/bin/env bash
# Puts every NPC back where the game's data has it, run as root on the server from the repository folder:
#   bash modules/mod-titan/reset-npcs.sh              shows what was moved (changes nothing)
#   bash modules/mod-titan/reset-npcs.sh --apply      moves them back and restarts the world server
#   add --gameobjects to do the same for objects (chairs, braziers, chests...)
#
# How: the db-import container builds a fresh copy of the world database (acore_world_ref: AzerothCore's data, its
# updates and our modules' spawns, exactly as a new server would have it). Every spawn in the live world whose map,
# position, facing, wander distance or movement differs from the copy gets the copy's (by spawn guid). NPCs someone
# added or deleted are only counted. The live tables are backed up to /root/backups first; the copy is dropped at the
# end. The world server keeps spawns in memory, so it restarts after the change (players are disconnected).
set -u
cd "$(dirname "$0")/../.." || exit 1

apply=0; objects=0
for arg in "$@"; do
    case "$arg" in
        --apply) apply=1 ;;
        --gameobjects) objects=1 ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

db() { docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -N -B acore_world 2>/dev/null'; }
dbt() { docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -t acore_world 2>/dev/null'; }
pass=$(docker exec ac-database printenv MYSQL_ROOT_PASSWORD) \
    || { echo "FAILED: the database container is not running."; exit 1; }

echo "== 1/4 Building a clean copy of the world (acore_world_ref, a few minutes)"
echo "DROP DATABASE IF EXISTS acore_world_ref;" | db
if ! docker compose run --rm --no-deps \
        -e AC_WORLD_DATABASE_INFO="ac-database;3306;root;${pass};acore_world_ref" \
        -e AC_UPDATES_ENABLE_DATABASES=4 -e AC_DISABLE_INTERACTIVE=1 \
        ac-db-import < /dev/null > /tmp/reset-npcs-import.log 2>&1; then
    echo "FAILED: the import stopped. Its log: /tmp/reset-npcs-import.log"; exit 1
fi
ref=$(echo "SELECT COUNT(*) FROM acore_world_ref.creature;" | db)
if [ -z "$ref" ] || [ "$ref" -lt 100000 ]; then
    echo "FAILED: the copy looks incomplete (${ref:-no} creature spawns). Its log: /tmp/reset-npcs-import.log"; exit 1
fi
echo "OK: $ref creature spawns in the clean copy."

# Spawns of a table (creature or gameobject) that differ from the copy's: 'where' and the columns put back.
moved_where() {
    local t=$1 extra=""
    [ "$t" = creature ] && extra="OR l.wander_distance <> r.wander_distance OR l.MovementType <> r.MovementType"
    echo "FROM acore_world.$t l JOIN acore_world_ref.$t r ON r.guid = l.guid
          WHERE l.map <> r.map OR ABS(l.position_x - r.position_x) > 0.05 OR ABS(l.position_y - r.position_y) > 0.05
             OR ABS(l.position_z - r.position_z) > 0.05 OR ABS(l.orientation - r.orientation) > 0.01 $extra"
}

echo
echo "== 2/4 What was moved"
for t in creature gameobject; do
    [ "$t" = gameobject ] && [ $objects = 0 ] && label=" (not reset without --gameobjects)" || label=""
    n=$(echo "SELECT COUNT(*) $(moved_where $t);" | db)
    added=$(echo "SELECT COUNT(*) FROM acore_world.$t l LEFT JOIN acore_world_ref.$t r ON r.guid = l.guid
                  WHERE r.guid IS NULL;" | db)
    gone=$(echo "SELECT COUNT(*) FROM acore_world_ref.$t r LEFT JOIN acore_world.$t l ON l.guid = r.guid
                 WHERE l.guid IS NULL;" | db)
    echo "$t: $n moved$label, $added added that the data does not have, $gone deleted."
done
echo "SELECT l.guid, t.name, l.map, ROUND(SQRT(POW(l.position_x - r.position_x, 2) + POW(l.position_y - r.position_y, 2)
      + POW(l.position_z - r.position_z, 2)), 1) AS yards_off
      FROM acore_world.creature l JOIN acore_world_ref.creature r ON r.guid = l.guid
      LEFT JOIN acore_world.creature_template t ON t.entry = l.id1
      WHERE l.map <> r.map OR ABS(l.position_x - r.position_x) > 0.05 OR ABS(l.position_y - r.position_y) > 0.05
         OR ABS(l.position_z - r.position_z) > 0.05 OR ABS(l.orientation - r.orientation) > 0.01
         OR l.wander_distance <> r.wander_distance OR l.MovementType <> r.MovementType
      ORDER BY yards_off DESC LIMIT 40;" | dbt

if [ $apply = 0 ]; then
    echo "DROP DATABASE IF EXISTS acore_world_ref;" | db
    echo
    echo "Nothing changed. To move them back: bash modules/mod-titan/reset-npcs.sh --apply"
    exit 0
fi

echo
echo "== 3/4 Backing up and moving them back"
mkdir -p /root/backups
backup=/root/backups/spawns-before-reset-$(date +%F-%H%M).sql.gz
docker exec ac-database sh -c 'mysqldump -uroot -p"$MYSQL_ROOT_PASSWORD" acore_world creature gameobject 2>/dev/null' \
    | gzip > "$backup"
if [ "$(stat -c %s "$backup")" -lt 100000 ]; then echo "FAILED: the backup is empty; nothing changed."; exit 1; fi
echo "OK: the current spawns are saved in $backup"
echo "UPDATE acore_world.creature l JOIN acore_world_ref.creature r ON r.guid = l.guid
      SET l.map = r.map, l.zoneId = r.zoneId, l.areaId = r.areaId, l.position_x = r.position_x,
          l.position_y = r.position_y, l.position_z = r.position_z, l.orientation = r.orientation,
          l.wander_distance = r.wander_distance, l.MovementType = r.MovementType, l.currentwaypoint = r.currentwaypoint;
      SELECT CONCAT('OK: ', ROW_COUNT(), ' NPCs moved back.');" | db
if [ $objects = 1 ]; then
    echo "UPDATE acore_world.gameobject l JOIN acore_world_ref.gameobject r ON r.guid = l.guid
          SET l.map = r.map, l.zoneId = r.zoneId, l.areaId = r.areaId, l.position_x = r.position_x,
              l.position_y = r.position_y, l.position_z = r.position_z, l.orientation = r.orientation,
              l.rotation0 = r.rotation0, l.rotation1 = r.rotation1, l.rotation2 = r.rotation2,
              l.rotation3 = r.rotation3;
          SELECT CONCAT('OK: ', ROW_COUNT(), ' objects moved back.');" | db
fi
echo "DROP DATABASE IF EXISTS acore_world_ref;" | db

echo
echo "== 4/4 Restarting the world server (spawns are read at startup)"
docker restart ac-worldserver > /dev/null && echo "OK: the world server is restarting; give it a minute."
