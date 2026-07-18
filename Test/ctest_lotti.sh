#!/usr/bin/env bash
# ctest_lotti.sh — esegue la suite di test a lotti per non surriscaldare la CPU.
#
# Uso:
#   ./ctest_lotti.sh          tutti i lotti in sequenza, con pausa termica tra un lotto e l'altro
#   ./ctest_lotti.sh 3        esegue solo il lotto 3
#   ./ctest_lotti.sh lista    elenca i lotti e i test che contengono
#
# Config (variabili d'ambiente):
#   LOTTO=10     test per lotto
#   SOGLIA=70    °C: sopra questa temperatura si aspetta prima di partire col lotto
#   ESCLUDI=""   regex ctest -E per saltare suite (es. "SttWhisper|PerceptorScripts")
#   BUILD=gui/build_tests   (build canonica dei test — build_gui ha BUILD_TESTS=OFF)

set -u
BUILD="${BUILD:-gui/build_tests}"
LOTTO="${LOTTO:-10}"
SOGLIA="${SOGLIA:-70}"
ESCLUDI="${ESCLUDI:-}"
# Lo script vive in Test/ — BUILD è relativo alla ROOT del repo (un livello sopra)
cd "$(dirname "$0")/../$BUILD" || { echo "❌  cartella $BUILD non trovata (build prima con cmake)"; exit 1; }

# numeri reali dei test (non partono necessariamente da 1)
mapfile -t NUMERI < <(ctest -N | sed -n 's/^ *Test *#\([0-9]*\):.*/\1/p')
TOT=${#NUMERI[@]}
[ "$TOT" -eq 0 ] && { echo "❌  nessun test trovato in $BUILD"; exit 1; }
NLOTTI=$(( (TOT + LOTTO - 1) / LOTTO ))

temp_cpu() { sensors 2>/dev/null | awk '/^Package id 0:/ {gsub(/[+°C]/,"",$4); print int($4); exit}'; }

attendi_raffreddamento() {
    local t
    while t=$(temp_cpu); [ -n "$t" ] && [ "$t" -ge "$SOGLIA" ]; do
        echo "    🌡  CPU a ${t}°C ≥ ${SOGLIA}°C — pausa 30s per raffreddare..."
        sleep 30
    done
}

esegui_lotto() {
    local n=$1 da quanti lista
    [ "$n" -ge 1 ] && [ "$n" -le "$NLOTTI" ] || { echo "❌  lotto $n inesistente (1-$NLOTTI)"; return 2; }
    da=$(( (n-1)*LOTTO )); quanti=$LOTTO
    lista=$(IFS=,; echo "${NUMERI[*]:$da:$quanti}")
    echo "══ Lotto $n/$NLOTTI ($(echo "$lista" | tr ',' ' ' | wc -w) test di $TOT) — CPU $(temp_cpu)°C ══"
    if [ -n "$ESCLUDI" ]; then
        nice -n19 ctest -I "0,0,,${lista}" -E "$ESCLUDI" -j1 --output-on-failure
    else
        nice -n19 ctest -I "0,0,,${lista}" -j1 --output-on-failure
    fi
}

case "${1:-}" in
    lista)
        echo "Suite totali: $TOT — $NLOTTI lotti da max $LOTTO test"
        ctest -N | sed -n 's/^ *Test *#\([0-9]*\): \(.*\)/\1: \2/p' |
        awk -v l="$LOTTO" '{ b=int((NR-1)/l)+1; if (b!=prev) { printf "\n— Lotto %d —\n", b; prev=b }; print "  " $0 }'
        ;;
    '')
        FALLITI=0
        for n in $(seq 1 "$NLOTTI"); do
            attendi_raffreddamento
            esegui_lotto "$n" || FALLITI=$((FALLITI+1))
        done
        echo
        echo "══ Finito: $NLOTTI lotti, $FALLITI lotti con fallimenti ══"
        [ "$FALLITI" -eq 0 ]
        ;;
    *)
        attendi_raffreddamento
        esegui_lotto "$1"
        ;;
esac
