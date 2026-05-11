#!/bin/bash
# SlougtHouse — Watchdog
# Monitort een rate-limit state file en herstart Claude zodra de reset-tijd verstreken is.

STATE_FILE="/Users/corstoof/SlougtHouse/.watchdog/rate_limit_state.json"
LOG_FILE="/Users/corstoof/SlougtHouse/.watchdog/watchdog.log"
CLAUDE="/Users/corstoof/.local/bin/claude"
WORKDIR="/Users/corstoof/SlougtHouse"

log() {
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

log "Watchdog gestart (PID $$)"

while true; do
  if [ -f "$STATE_FILE" ]; then
    RESET_EPOCH=$(python3 -c "import json; d=json.load(open('$STATE_FILE')); print(d.get('reset_epoch',0))" 2>/dev/null)
    NOW=$(date +%s)

    if [ -n "$RESET_EPOCH" ] && [ "$NOW" -ge "$RESET_EPOCH" ]; then
      TAAK=$(python3 -c "import json; d=json.load(open('$STATE_FILE')); print(d.get('taak','lopende taken'))" 2>/dev/null)
      log "Rate limit gereset. Herstart voor taak: $TAAK"

      rm -f "$STATE_FILE"

      cd "$WORKDIR"
      "$CLAUDE" --dangerously-skip-permissions -p "Rate limit is gereset. Hervat de volgende taak van SlougtHouse: $TAAK" \
        >> "$LOG_FILE" 2>&1 &

      log "Claude gestart (PID $!)"
    fi
  fi

  sleep 60
done
