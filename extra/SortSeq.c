#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINHA 5000 // Tamanho máximo que uma linha pode ter
#define MAX_SEQS 10000000 // Aumenta a capacidade inicial

// Função simples para criar diretório se não existir
void cria_pasta_se_preciso(const char *nome) {
    mkdir(nome, 0777);
}

// Função de comparação para qsort
int compara_dna(const void *a, const void *b) {
  // Compara as strings de DNA
  return strcmp(*(const char **)a, *(const char **)b);
}

// Função para ler o arquivo e retornar um array de strings
char **le_arquivo_dna(const char *nome_arq, int *n_seqs) {
  FILE *arq = fopen(nome_arq, "r");
  if (!arq) {
    perror("Erro ao abrir arquivo de entrada");
    return NULL;
  }

  char linha[MAX_LINHA];
  int capacidade = 10000; // Capacidade inicial
  int count = 0;

  char **sequencias = (char **)malloc(capacidade * sizeof(char *));

  while (fgets(linha, MAX_LINHA, arq)) {
    // Remove o '\n' do final
    linha[strcspn(linha, "\n")] = '\0';
    
    // Se precisar de mais espaço, dobra a capacidade
    if (count >= capacidade) {
      capacidade *= 2;
      sequencias = (char **)realloc(sequencias, capacidade * sizeof(char *));
    }
    
    // Salva a string (alocando a memória exata)
    sequencias[count] = strdup(linha); 
    count++;
  }

  fclose(arq);
  *n_seqs = count;
  return sequencias;
}

// Função para liberar a memória
void libera_sequencias(char **seqs, int n) {
  if (seqs == NULL) return;
  for (int i = 0; i < n; i++) {
    free(seqs[i]);
  }
  free(seqs);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Uso: %s <caminho_entrada> <nome_arquivo_saida>\n", argv[0]);
    return 1;
  }

  const char *arq_entrada = argv[1];
  const char *nome_arq_saida = argv[2];

  int n = 0;
  char **sequencias = le_arquivo_dna(arq_entrada, &n);
  if (!sequencias) return 1;
  
  printf("Lidas %d sequencias. Começando a ordenar...\n", n);

  // --- Medição de Tempo e Ordenação ---
  clock_t inicio = clock();
  qsort(sequencias, n, sizeof(char *), compara_dna);
  clock_t fim = clock();
  double tempo = ((double)(fim - inicio)) / CLOCKS_PER_SEC;

  printf("Ordenacao sequencial concluída. Tempo: %.6f segundos\n", tempo);
  
  // --- Escrita do Resultado ---
  cria_pasta_se_preciso("output");
  char caminho_saida[256];
  sprintf(caminho_saida, "output/%s", nome_arq_saida);

  FILE *arq_saida = fopen(caminho_saida, "w");
  if (!arq_saida) {
    perror("Erro ao salvar arquivo de saída");
    libera_sequencias(sequencias, n);
    return 1;
  }

  for (int i = 0; i < n; i++) {
    fprintf(arq_saida, "%s\n", sequencias[i]);
  }
  fclose(arq_saida);
  printf("Resultado salvo em %s\n", caminho_saida);

  libera_sequencias(sequencias, n);

  return 0;
}
