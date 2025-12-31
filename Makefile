CC = gcc
# CFLAGS : On ajoute -I. pour dire "cherche aussi depuis le dossier racine"
CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0` -I.
LIBS = `pkg-config --libs gtk+-3.0` -lcrypto

# Trouve tous les fichiers .c dans le dossier src
SRCS = $(wildcard src/*.c)
# Crée la liste des fichiers .o correspondants (ex: src/main.o)
OBJS = $(SRCS:.c=.o)

# Nom de votre programme final
TARGET = resto

# La cible 'all' (par défaut) dépend de votre programme final
all: $(TARGET)

# Règle pour créer le programme final (le 'linking')
# Il dépend des fichiers objets (.o)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Règle "magique" (pattern rule) pour transformer n'importe quel .c en .o
# -c signifie "compiler seulement, ne pas lier"
# $< est le fichier .c (la source)
# $@ est le fichier .o (la cible)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Règle pour tout nettoyer
clean:
	rm -f $(TARGET) $(OBJS)