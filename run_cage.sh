#!/bin/bash
# Wait for Cage to be ready
sleep 2

# Try to find HDMI outputs
OUT1=$(wlr-randr | grep "^HDMI-A" | head -n 1 | cut -d' ' -f1)
OUT2=$(wlr-randr | grep "^HDMI-A" | tail -n 1 | cut -d' ' -f1)

if [ -n "$OUT1" ] && [ -n "$OUT2" ] && [ "$OUT1" != "$OUT2" ]; then
    echo "Configuring $OUT1 (Primary) and $OUT2 (Flipped Mirror)"
    wlr-randr --output "$OUT1" --pos 0,0 --output "$OUT2" --pos 0,0 --transform flipped
else
    echo "Only one monitor or no HDMI monitors detected. Running standard."
fi

# Execute the actual application
./build/373-telescope
