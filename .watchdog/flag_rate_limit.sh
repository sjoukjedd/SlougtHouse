#!/bin/bash
# Gebruik: flag_rate_limit.sh "15:10" "omschrijving van de taak"

RESET_TIME="$1"
TAAK="${2:-lopende taken van SlougtHouse hervatten}"
STATE_FILE="/Users/corstoof/SlougtHouse/.watchdog/rate_limit_state.json"

if [ -z "$RESET_TIME" ]; then
  echo "Gebruik: $0 <reset-tijd HH:MM> [taakbeschrijving]"
  exit 1
fi

RESET_EPOCH=$(python3 -c "
import datetime
t = '$RESET_TIME'
now = datetime.datetime.now()
h, m = map(int, t.split(':'))
reset = now.replace(hour=h, minute=m, second=0, microsecond=0)
if reset <= now:
    reset += datetime.timedelta(days=1)
print(int(reset.timestamp()))
")

python3 -c "
import json
data = {'reset_epoch': $RESET_EPOCH, 'reset_time': '$RESET_TIME', 'taak': '''$TAAK'''}
with open('$STATE_FILE', 'w') as f:
    json.dump(data, f, indent=2)
print('State opgeslagen. Watchdog herstart om $RESET_TIME.')
"
