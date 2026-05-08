#!/bin/sh
# Interactive LED test — run from host, sends commands via POST /led
# You watch the board's LEDs and tell me y/n for each.

HOST="http://192.168.11.1:8080/led"
PASS=0
FAIL=0

test_led() {
	local wan=$1 lan=$2 wifi=$3 desc="$4"

	printf "\n[PASS:%d FAIL:%d] %s\n" "$PASS" "$FAIL" "$desc"
	printf "  Sending: wan=%d lan=%d wifi=%d\n" "$wan" "$lan" "$wifi"
	curl -s -X POST "$HOST" \
		-H 'Content-Type: application/json' \
		-d "{\"wan\":$wan,\"lan\":$lan,\"wifi\":$wifi}" | sed 's/^/  Response: /'

	while :; do
		printf "  Correct? (y/n/q): "
		read -r ans
		case "$ans" in
			y|Y) PASS=$((PASS+1)); break ;;
			n|N) FAIL=$((FAIL+1)); break ;;
			q|Q) return 1 ;;
			*) continue ;;
		esac
	done
}

test_led 2 1 1 "Only Wan On"
test_led 1 2 1 "Only Lan On"
test_led 1 1 2 "Only Wifi On"
test_led 2 2 2 "All On"
test_led 1 1 1 "All Off"
test_led 3 1 1 "Wan Slow Flash, others Off"
test_led 1 3 1 "Lan Slow Flash, others Off"
test_led 1 1 3 "Wifi Slow Flash, others Off"
test_led 4 1 1 "Wan Fast Flash, others Off"
test_led 2 3 4 "Mixed: Wan=On Lan=Slow Wifi=Fast"
test_led 15 15 15 "All Keep (no change)"

printf "\n====================\n"
printf "Passed: %d\n" "$PASS"
printf "Failed: %d\n" "$FAIL"
printf "====================\n"
