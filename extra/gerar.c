#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define TAM_SEQ 50
#define DNA_CARACTERES "ACGT"

// Função simples para criar diretório se não existir
void cria_pasta_se_preciso(const char *nome) {
    mkdir(nome, 0777); // Tenta criar. Se já existe, não tem problema.
}

// Gera uma sequência de DNA
void gera_sequencia_dna(char *buffer, int tamanho) {
  for (int i = 0; i < tamanho; i++) {
    buffer[i] = DNA_CARACTERES[rand() % 4];
  }
  buffer[tamanho] = '\0';
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Uso: %s <num_sequencias> <nome_arquivo_saida>\n", argv[0]);
    return 1;
  }

  int n = atoi(argv[1]);
  const char *nome_arquivo = argv[2];
  
  srand(time(NULL));

  cria_pasta_se_preciso("input");

  char caminho_completo[256];
  sprintf(caminho_completo, "input/%s", nome_arquivo);

  FILE *arq = fopen(caminho_completo, "w");
  if (!arq) {
    perror("Erro ao criar arquivo");
    return 1;
  }

  char seq_buffer[TAM_SEQ + 1];
  for (int i = 0; i < n; i++) {
    gera_sequencia_dna(seq_buffer, TAM_SEQ);
    fprintf(arq, "%s\n", seq_buffer);
  }

  fclose(arq);
  printf("Geradas %d sequencias em %s\n", n, caminho_completo);

  return 0;
}
