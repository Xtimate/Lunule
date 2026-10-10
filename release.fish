#!/usr/bin/env fish
# release.fish
rm -rf release; and mkdir -p release
python compiler/main.py examples/showcase/stats.lun -o release/stats; or exit 1
pyinstaller --onefile --name lunule --paths compiler --add-data "runtime:runtime" compiler/main.py; or exit 1
cp dist/lunule release/lunule
cp README.md release/
cp -r examples release/examples
tar czf lunule-linux-x86_64.tar.gz release
echo "build lunule-linux-x86_64.tar.gz"
