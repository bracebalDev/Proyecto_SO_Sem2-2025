# --- Makefile — Eco-Flow 2026 ---
# Autores: María Laura Gutiérrez, Rebeca Blanco, Andrés Crespo, Brayan Ceballos
# Fecha: 9 de Marzo de 2026
#
# Uso:
#   make          → Compila el proyecto
#   make run      → Compila y ejecuta
#   make clean    → Elimina obj/ y el ejecutable (no quedan .o sueltos)
#   make DEBUG=1  → Compila con símbolos de depuración (-g)

TARGET  = eco_flow_app
CC      = gcc

# Flags base: warnings estrictos, cabeceras en include/, soporte pthreads
CFLAGS  = -Wall -Wextra -I./include -pthread

# Flag de debug opcional: make DEBUG=1 agrega -g y desactiva optimizaciones
ifdef DEBUG
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2
endif

# Directorios
SRC_DIR = src
OBJ_DIR = obj

# Fuentes y objetos (los .o NUNCA van en src/, siempre en obj/)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# ── Regla principal ─────────────────────────────────────────────────
all: $(TARGET)

# ── Enlazado ────────────────────────────────────────────────────────
$(TARGET): $(OBJS)
	@echo "  [LINK]  $@"
	$(CC) $(CFLAGS) -o $@ $^
	@echo "  Listo: ./$@"

# ── Compilación de cada .c a su .o dentro de obj/ ───────────────────
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "  [CC]    $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ── Crear carpeta obj/ si no existe ─────────────────────────────────
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ── Limpiar todo lo generado (make clean) ───────────────────────────
clean:
	@echo "  [CLEAN] Eliminando obj/ y $(TARGET)..."
	rm -rf $(OBJ_DIR) $(TARGET)

# ── Compilar y ejecutar (make run) ──────────────────────────────────
run: $(TARGET)
	@echo "--- Ejecutando Eco-Flow 2026 ---"
	./$(TARGET)

.PHONY: all clean run