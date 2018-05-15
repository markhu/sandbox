
# rn-clean-node-modules.sh

set -x
                       rm -rf node_modules yarn.lock package-lock.json
cd packages/mobile  && rm -rf node_modules yarn.lock package-lock.json
cd ../state-manager && rm -rf node_modules yarn.lock package-lock.json
set +x
echo yarn setup ios

