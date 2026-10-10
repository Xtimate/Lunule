#!/usr/bin/env fish
# release.fish
rm -rf release; and mkdir -p release
python compiler/main.py examples/showcase/stats.lang -o release/stats; or exit 1
pyinstaller --onefile --name langc --paths compiler --add-data "runtime:runtime" compiler/main.py; or exit 1
cp dist/langc release/langc
cp README.md release/
cp -r examples release/examples
tar czf LANGNAME-linux-x86_64.tar.gz release
echo "build LANGNAME-linux-x86_64.tar.gz"
