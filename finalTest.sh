#!/bin/bash

# Compila tus programas
make

# Abre una nueva terminal para mainServer
gnome-terminal --geometry=65x20+0+0 --title="mainServer" -- ./mainServer

# Abre 2 nuevas terminales para subServer en la parte superior
for i in {1..2} 
do
    gnome-terminal --geometry=65x20+$((i*700))+0 --title="subServer $i (top)" -- ./subServer
done

# Abre 2 nuevas terminales para subServer en la parte inferior
for i in {1..2}
do
    gnome-terminal --geometry=65x20+$((i*700))+550 --title="subServer $i (bottom)" -- ./subServer
done

# Abre una nueva terminal para client en la esquina inferior derecha
gnome-terminal --geometry=65x20+0+550 --title="client" -- ./client


# TXT
sleep 4  # Adjust the sleep duration (in seconds) as needed

while read -r line; do
    $line
done < Flavor_network3.txt


# Para ejecutar el programa, ejecuta el siguiente comando en la terminal:
# chmod +x run.sh
# A partir de ahora, puedes ejecutar el programa con el siguiente comando:
# ./run.sh