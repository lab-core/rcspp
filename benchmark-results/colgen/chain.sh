#!/bin/bash
# Waits for the large run to stop or finish, then starts after_large.sh.
cd /c/Users/marbe/Desktop/programming/git/rcspp
until grep -qE "^(stopped|finished)" benchmark-results/colgen/large/status.txt 2>/dev/null; do sleep 30; done
echo "large run ended: $(head -1 benchmark-results/colgen/large/status.txt)"
exec benchmark-results/colgen/after_large.sh
