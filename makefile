# --- Makefile ---
# Autor: María Laura Gutiérrez (V-), Rebeca Blanco (V-), Andrés Crespo (V-), Brayan Ceballos
# Fecha: 9 de Marzo de 2026

# Nombre del ejecutable final
TARGET = eco_flow_app

# Compilador y banderas
CC = gcc
# -I./include: Busca archivos .h en la carpeta include
# -pthread: Necesario para hilos POSIX (forks/threads)
CFLAGS = -Wall -Wextra -I./include -pthread

# Directorios
SRC_DIR = src
OBJ_DIR = obj

# Encontrar todos los archivos .c en src/
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Convertir nombres .c a .o
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Regla principal (lo que pasa cuando escribes 'make')
all: $(TARGET)

# Enlazado final
$(TARGET): $(OBJS)
	@echo "Enlazando ejecutable: $@"
	$(CC) $(CFLAGS) -o $@ $^

# Compilación de objetos individuales
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compilando: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Crear carpeta de objetos si no existe
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Limpiar archivos generados (make clean)
clean:
	@echo "Limpiando archivos temporales..."
	rm -rf $(OBJ_DIR) $(TARGET)

# Ejecutar el programa (make run)
run: $(TARGET)
	@echo "--- Ejecutando Eco Flow ---"
	./$(TARGET)

.PHONY: all clean run