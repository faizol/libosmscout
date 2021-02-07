#!/bin/bash -e

TOP_DIR=$(readlink -f "$(dirname $0)/..")
BUILD_DIR=${BUILD_DIR:-"${TOP_DIR}/cmake-build-debug"}
ROUTING_DEMO=${ROUTING_DEMO:-"${BUILD_DIR}/Demos/Routing"}

if [ ! -x "$ROUTING_DEMO" ] ; then
  echo "Cannot find routing demo executable: $ROUTING_DEMO" >&2
  exit 1
fi

jq --version > /dev/null
if [ $? -ne 0 ] ; then
  echo "Cannot find jq tool" >&2
  exit 1
fi

MAP_BASE_DIR=${MAP_BASE_DIR:-"$HOME/Maps"}
CZECH_DATABASE=${CZECH_DATABASE:-"$(ls -d1 "${MAP_BASE_DIR}/europe-czech-republic"* | tail -1)"}
GERMAN_DATABASE=${GERMAN_DATABASE:-"$(ls -d1 "${MAP_BASE_DIR}/europe-germany"* | tail -1)"}

if [ ! -f "${CZECH_DATABASE}/types.dat" ] ; then
  echo "Cannot find Czech database: $CZECH_DATABASE" >&2
  exit 1
fi
if [ ! -f "${GERMAN_DATABASE}/types.dat" ] ; then
  echo "Cannot find Czech database: $GERMAN_DATABASE" >&2
  exit 1
fi

function verifyRouting() {
  REFERENCE_JSON="$1"
  echo "Verifying route $(basename --suffix='.json' "$REFERENCE_JSON")"
  shift
  TEST_OUTPUT=$(mktemp --suffix='.json')
  "$ROUTING_DEMO" --routeJson "$TEST_OUTPUT" "$@" > /dev/null || exit 1
  if [ $(jq --argfile reference "$REFERENCE_JSON" --argfile test "$TEST_OUTPUT" -n '$reference == $test') != "true" ] ; then
    echo "Route don't match to $REFERENCE_JSON" >&2
    rm "$TEST_OUTPUT"
    exit 1
  fi
  rm "$TEST_OUTPUT"
}

verifyRouting "${TOP_DIR}/Tests/data/routing/srbsko.json"          "${CZECH_DATABASE}" 49.9934 14.1575 49.9469 14.1402
verifyRouting "${TOP_DIR}/Tests/data/routing/harrachov-praha.json" "${CZECH_DATABASE}" 50.7728 15.4261 50.1004 14.4453
verifyRouting "${TOP_DIR}/Tests/data/routing/sumburk.json"         "${CZECH_DATABASE}" 50.7509 15.3151 50.7335 15.3000

echo "Done"
