#!/bin/bash
# WD My Cloud EX2 — autorun script
# Place at: /mnt/HD/HD_a2/Nas_Prog/autorun.sh
# csv_server.py must be at: /shares/Public/Kwal/csv_server.py
#
# To install:
#   ssh sshd@<NAS-IP>
#   mkdir -p /mnt/HD/HD_a2/Nas_Prog
#   cp /shares/Public/Kwal/nas_autorun.sh /mnt/HD/HD_a2/Nas_Prog/autorun.sh
#   chmod 755 /mnt/HD/HD_a2/Nas_Prog/autorun.sh
#   cp /shares/Public/Kwal/csv_server.py /shares/Public/Kwal/csv_server.py
#   chmod 755 /shares/Public/Kwal/csv_server.py

LOG="/shares/Public/Kwal/csv_server.log"
SCRIPT="/shares/Public/Kwal/csv_server.py"
PYTHON="/usr/bin/python3"

# Wait for shares to be mounted
sleep 30

if [ ! -f "$SCRIPT" ]; then
    echo "$(date): csv_server.py not found at $SCRIPT" >> "$LOG"
    exit 1
fi

if [ ! -x "$PYTHON" ]; then
    # Try alternative locations
    PYTHON=$(which python3 2>/dev/null)
    if [ -z "$PYTHON" ]; then
        echo "$(date): python3 not found" >> "$LOG"
        exit 1
    fi
fi

# Kill any existing instance
pkill -f "csv_server.py" 2>/dev/null

echo "$(date): Starting csv_server.py" >> "$LOG"
nohup "$PYTHON" "$SCRIPT" >> "$LOG" 2>&1 &
echo "$(date): csv_server.py started (PID $!)" >> "$LOG"
