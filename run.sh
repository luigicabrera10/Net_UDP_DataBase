#!/bin/bash

# Compila tus programas
make

# Abre una nueva terminal para mainServer
gnome-terminal --geometry=65x20+0+0 -- ./mainServer

# Abre 2 nuevas terminales para subServer en la parte superior
for i in {1..2} 
do
    gnome-terminal --geometry=65x20+$((i*700))+0 -- ./subServer
done

# Abre 2 nuevas terminales para subServer en la parte inferior
for i in {1..2}
do
    gnome-terminal --geometry=65x20+$((i*700))+550 -- ./subServer
done

# Abre una nueva terminal para client en la esquina inferior derecha
gnome-terminal --geometry=65x20+0+550 -- ./client

# Para ejecutar el programa, ejecuta el siguiente comando en la terminal:
# chmod +x run.sh
# A partir de ahora, puedes ejecutar el programa con el siguiente comando:
# ./run.sh