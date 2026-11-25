#!/bin/bash


echo "    INICIANDO SIMULACIÓN A ESCALA: PROYECTO BLACKOUT"


rm -f tienda_*_blackout.log

./tienda_node 1 1 8001 30000.0 > log_node1.txt 2>&1 &
PID1=$!
echo "[OK] Tienda 1 (Cadena) iniciada (PID: $PID1) en puerto 8001"
sleep 1

./tienda_node 2 1 8002 30000.0 127.0.0.1 8001 > log_node2.txt 2>&1 &
PID2=$!
echo "[OK] Tienda 2 (Cadena) iniciada (PID: $PID2) en puerto 8002"
sleep 1

./tienda_node 3 1 8003 30000.0 127.0.0.1 8001 > log_node3.txt 2>&1 &
PID3=$!
echo "[OK] Tienda 3 (Cadena) iniciada (PID: $PID3) en puerto 8003"
sleep 1

./tienda_node 4 0 8004 10000.0 127.0.0.1 8001 > log_node4.txt 2>&1 &
PID4=$!
echo "[OK] Tienda 4 (Independiente) iniciada (PID: $PID4) en puerto 8004"
sleep 2

echo " INYECTANDO TRÁFICO CONCURRENTE CON EL SIMULADOR..."

./simulador 1 | ./tienda_node 5 1 8005 30000.0 127.0.0.1 8001 > log_node5.txt 2>&1 &
SIM_PID=$!

sleep 8

echo " SIMULANDO CORTE DE ENERGÍA REPENTINO EN TIENDA 2..."

kill -9 $PID2 2>/dev/null || true
echo "[APAGÓN] Tienda 2 desconectada forzosamente."

sleep 5

echo " AUDITORÍA DE CONSISTENCIA Y BITÁCORAS DE DISCO"
echo "Contenido persistido en disco para Tienda 1 (fsync):"
cat tienda_1_blackout.log | tail -n 5 2>/dev/null || echo "No hay registros."

kill -9 $PID1 $PID3 $PID4 $SIM_PID 2>/dev/null || true
echo "          PRUEBA A ESCALA FINALIZADA"