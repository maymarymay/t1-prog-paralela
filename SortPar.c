#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/stat.h>

#define TAM_SEQ 20 //mudar o tamanho da sequencia
#define TAM_CHUNK (TAM_SEQ + 1)
#define MAX_LINHA 5000 
#define OUTPUT_DIR "output"

//se precisar cria a pasta pra nao dar erro de diretorio nao encontrado
void cria_pasta(const char *nome) {
    mkdir(nome, 0777);
}

//funçao comparaçao pro qsort
int compara_dna(const void *a, const void *b) {
    return strcmp(*(char**)a, *(char**)b);
}

//liberaçao de memoria do array de string
void libera_strings(char** array, int n) {
    if (array == NULL) return;
    for (int i = 0; i < n; i++) {
        free(array[i]);
    }
    free(array);
}

//lendo arquivo
char **le_arquivo_rank0(const char *nome_arq, int *n_total) {
  FILE *arq = fopen(nome_arq, "r");
  if (!arq) {
    perror("Não deu pra ler o arquivo :(\n");
    return NULL;
  }
  char linha[MAX_LINHA];
  int capacidade = 10000;
  int count = 0;
  char **seqs = (char **)malloc(capacidade * sizeof(char *));

  while (fgets(linha, MAX_LINHA, arq)) {
    linha[strcspn(linha, "\n")] = '\0';
    if (count >= capacidade) {
      capacidade *= 2;
      seqs = (char **)realloc(seqs, capacidade * sizeof(char *));
    }
    seqs[count] = strdup(linha); 
    count++;
  }

  fclose(arq);
  *n_total = count;
  return seqs;
}

//screve o array ordenado no arquivo de saída
void escreve_arquivo_rank0(const char *nome_final, char **seqs, int n_total) {
  cria_pasta(OUTPUT_DIR); 

  char caminho_completo[256];
  sprintf(caminho_completo, "%s/%s", OUTPUT_DIR, nome_final);

  FILE *arq = fopen(caminho_completo, "w");
  if (!arq) {
    perror("Erro criando arquivo de saída :(\n");
    return;
  }
  for (int i = 0; i < n_total; i++) {
    fprintf(arq, "%s\n", seqs[i]);
  }
  fclose(arq);
  printf("Salvo em %s\n", caminho_completo);
}


//split sort
void ordena_paralela_split_sort(char*** local_array_ptr, int* local_count_ptr, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    if (*local_count_ptr == 0) {
        return;
    }

    char** local_array = *local_array_ptr;
//ord inicial
    qsort(local_array, *local_count_ptr, sizeof(char*), compara_dna);

    int num_splitters = size - 1;
    char splitters_locais_flat[num_splitters * TAM_CHUNK];
//selecao
    for (int i = 0; i < num_splitters; i++) {
        int index = (i + 1) * (*local_count_ptr) / size;
        if (index >= *local_count_ptr) index = *local_count_ptr - 1;
        strcpy(splitters_locais_flat + i * TAM_CHUNK, local_array[index]);
    }

    char* todos_splitters_flat = NULL;
    if (rank == 0) {
        todos_splitters_flat = malloc(size * num_splitters * TAM_CHUNK);
    }
//o.o
//coloca no rank 0
    MPI_Gather(splitters_locais_flat, num_splitters * TAM_CHUNK, MPI_CHAR,
               todos_splitters_flat, num_splitters * TAM_CHUNK, MPI_CHAR, 0, comm);

    char splitters_globais_flat[num_splitters * TAM_CHUNK];
    if (rank == 0) {
//vivaaa simplificacao
        for (int i = 0; i < num_splitters; i++) {
            strcpy(splitters_globais_flat + i * TAM_CHUNK, 
                   todos_splitters_flat + i * TAM_CHUNK);
        }
        free(todos_splitters_flat);
    }
//broadcast
    MPI_Bcast(splitters_globais_flat, num_splitters * TAM_CHUNK, MPI_CHAR, 0, comm);

//alltoall
    int* envia_contagens_itens = calloc(size, sizeof(int));
    for (int i = 0; i < *local_count_ptr; i++) {
        char* elemento = local_array[i];
        int destino = 0;
//aqui ta determinando o destino
        while (destino < num_splitters && 
               strcmp(elemento, splitters_globais_flat + destino * TAM_CHUNK) > 0) {
            destino++;
        }
        envia_contagens_itens[destino]++;
    }

    int* recebe_contagens_itens = malloc(size * sizeof(int));
    MPI_Alltoall(envia_contagens_itens, 1, MPI_INT, recebe_contagens_itens, 1, MPI_INT, comm);

    int total_receber_itens = 0;
    for(int i = 0; i < size; i++) total_receber_itens += recebe_contagens_itens[i];

    int *envia_contagens_bytes = malloc(size * sizeof(int));
    int *recebe_contagens_bytes = malloc(size * sizeof(int));
    int *envia_desloc_bytes = malloc(size * sizeof(int));
    int *recebe_desloc_bytes = malloc(size * sizeof(int));
    
    envia_desloc_bytes[0] = recebe_desloc_bytes[0] = 0;
    for (int i = 0; i < size; i++) {
        envia_contagens_bytes[i] = envia_contagens_itens[i] * TAM_CHUNK;
        recebe_contagens_bytes[i] = recebe_contagens_itens[i] * TAM_CHUNK;
    }
    for (int i = 1; i < size; i++) {
        envia_desloc_bytes[i] = envia_desloc_bytes[i - 1] + envia_contagens_bytes[i - 1];
        recebe_desloc_bytes[i] = recebe_desloc_bytes[i - 1] + recebe_contagens_bytes[i - 1];
    }
    
    int total_enviar_bytes = envia_desloc_bytes[size-1] + envia_contagens_bytes[size-1];
    char* buffer_envio = malloc(total_enviar_bytes);
    int* contagens_temp = calloc(size, sizeof(int));
//preenchendo o bufferr
    for (int i = 0; i < *local_count_ptr; i++) {
        char* elemento = local_array[i];
        int destino = 0;
        while (destino < num_splitters && strcmp(elemento, splitters_globais_flat + destino * TAM_CHUNK) > 0) {
            destino++;
        }
        int pos_relativa = contagens_temp[destino] * TAM_CHUNK; 
        memcpy(buffer_envio + envia_desloc_bytes[destino] + pos_relativa, elemento, TAM_CHUNK);
        contagens_temp[destino]++;
    }

    char* buffer_recebimento = malloc(total_receber_itens * TAM_CHUNK);
//é isso ai 
    MPI_Alltoallv(buffer_envio, envia_contagens_bytes, envia_desloc_bytes, MPI_CHAR,
                  buffer_recebimento, recebe_contagens_bytes, recebe_desloc_bytes, MPI_CHAR, comm);

    char** novo_array_local = malloc(total_receber_itens * sizeof(char*));
    for (int i = 0; i < total_receber_itens; i++) {
        novo_array_local[i] = malloc(TAM_CHUNK);
        memcpy(novo_array_local[i], buffer_recebimento + i * TAM_CHUNK, TAM_CHUNK);
    }
    
    libera_strings(local_array, *local_count_ptr);
    *local_array_ptr = novo_array_local;
    *local_count_ptr = total_receber_itens;
//ordenaçao local
    qsort(novo_array_local, *local_count_ptr, sizeof(char*), compara_dna);
    
    free(buffer_recebimento);
    free(buffer_envio); 
    free(envia_contagens_itens); 
    free(recebe_contagens_itens); 
    free(envia_contagens_bytes); 
    free(recebe_contagens_bytes); 
    free(envia_desloc_bytes); 
    free(recebe_desloc_bytes); 
    free(contagens_temp);
}

//MAINN :D
int main(int argc, char* argv[]) {
    int rank, size;
    char** array_local = NULL;
    int count_local = 0;
    int n_total = 0;
    double tempo_inicio, tempo_fim;
    
    int n_final = 0; 
    char **array_final_ordenado = NULL; 
    char **array_global = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size); 

    if (argc != 3) {
        if (rank == 0) {
            printf("tá faltando coisa... ou tem coisa de mais\n");
            printf("lembra!!! mpirun -np [num de processadores] entrada.txt saida.txt\n");
            printf("se não rodar só colocar '--oversubscribe' depois do num de processadores\n");
        }
        MPI_Finalize();
        return 1;
    }

    const char *arq_entrada = argv[1];
    const char *nome_arq_saida_base = argv[2]; 

//leitura
    if (rank == 0) {
        array_global = le_arquivo_rank0(arq_entrada, &n_total);
        if (!array_global) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("Leu %d sequências :o uau\n", n_total);
    }
    
    MPI_Bcast(&n_total, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (n_total == 0) {
        MPI_Finalize();
        return 0;
    }

//distribuindo
    int *contagens_itens = malloc(size * sizeof(int));
    int *desloc_itens = malloc(size * sizeof(int));
    int desloc_atual = 0;

    for (int i = 0; i < size; i++) {
        contagens_itens[i] = n_total / size;
        if (i < n_total % size) {
            contagens_itens[i]++;
        }
        desloc_itens[i] = desloc_atual;
        desloc_atual += contagens_itens[i];
    }
    count_local = contagens_itens[rank];
    array_local = malloc(count_local * sizeof(char*));

    char *buffer_envio_flat = NULL;
    char *buffer_recebe_flat = malloc(count_local * TAM_CHUNK);
    
    if (rank == 0) {
        buffer_envio_flat = malloc(n_total * TAM_CHUNK);
//tuc
        for(int i = 0; i < n_total; i++) {
            memcpy(buffer_envio_flat + i * TAM_CHUNK, array_global[i], TAM_CHUNK);
        }
        libera_strings(array_global, n_total); 
        
        tempo_inicio = MPI_Wtime(); 
    }

    int *contagens_bytes = malloc(size * sizeof(int));
    int *desloc_bytes = malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        contagens_bytes[i] = contagens_itens[i] * TAM_CHUNK;
        desloc_bytes[i] = desloc_itens[i] * TAM_CHUNK;
    }

    MPI_Scatterv(buffer_envio_flat, contagens_bytes, desloc_bytes, MPI_CHAR,
                 buffer_recebe_flat, count_local * TAM_CHUNK, MPI_CHAR, 0, MPI_COMM_WORLD);
    
    if (rank == 0) free(buffer_envio_flat);
//distuc
    for (int i = 0; i < count_local; i++) {
        array_local[i] = malloc(TAM_CHUNK);
        memcpy(array_local[i], buffer_recebe_flat + i * TAM_CHUNK, TAM_CHUNK);
    }
    free(buffer_recebe_flat);
    free(contagens_bytes);
    free(desloc_bytes);
    
//ordenaçao
    ordena_paralela_split_sort(&array_local, &count_local, MPI_COMM_WORLD);
    
    if (rank == 0) {
        tempo_fim = MPI_Wtime();
        printf("ACABOU!!!!! ordenação paralela em %.6f segundos com %d processadores!!\n", tempo_fim-tempo_inicio, size);
    }

//coleta
    int *contagens_finais_itens = malloc(size * sizeof(int));
    MPI_Gather(&count_local, 1, MPI_INT, contagens_finais_itens, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int *contagens_finais_bytes = NULL;
    int *desloc_finais_bytes = NULL;
    char *buffer_recebe_global = NULL;
    
    if (rank == 0) {
        int *desloc_finais_itens = malloc(size * sizeof(int));
        desloc_finais_itens[0] = 0;
        n_final = contagens_finais_itens[0];
//calculadno deslocamento final e total final
        for (int i = 1; i < size; i++) {
            desloc_finais_itens[i] = desloc_finais_itens[i - 1] + contagens_finais_itens[i - 1];
            n_final += contagens_finais_itens[i];
        }

        contagens_finais_bytes = malloc(size * sizeof(int));
        desloc_finais_bytes = malloc(size * sizeof(int));
        for (int i = 0; i < size; i++) {
            contagens_finais_bytes[i] = contagens_finais_itens[i] * TAM_CHUNK;
            desloc_finais_bytes[i] = desloc_finais_itens[i] * TAM_CHUNK;
        }
        free(desloc_finais_itens);
        buffer_recebe_global = malloc(n_final * TAM_CHUNK);
    }
//tuc
    char *buffer_envia_local = malloc(count_local * TAM_CHUNK);
    for (int i = 0; i < count_local; i++) {
        memcpy(buffer_envia_local + i * TAM_CHUNK, array_local[i], TAM_CHUNK);
    }
    
    MPI_Gatherv(buffer_envia_local, count_local * TAM_CHUNK, MPI_CHAR,
                buffer_recebe_global, contagens_finais_bytes, desloc_finais_bytes, MPI_CHAR, 0, MPI_COMM_WORLD);
    
    free(buffer_envia_local);
    libera_strings(array_local, count_local);
    
//escrita
    if (rank == 0) {
        array_final_ordenado = malloc(n_final * sizeof(char*));
        for (int i = 0; i < n_final; i++) {
            array_final_ordenado[i] = malloc(TAM_CHUNK);
            memcpy(array_final_ordenado[i], buffer_recebe_global + i * TAM_CHUNK, TAM_CHUNK);
        }
        
        char nome_final[256];
        char *ponto = strrchr(nome_arq_saida_base, '.');

        if (ponto) {
            int len = ponto - nome_arq_saida_base;
            strncpy(nome_final, nome_arq_saida_base, len); 
            sprintf(nome_final + len, "_p%d%s", size, ponto); 
        } else {
            sprintf(nome_final, "%s_p%d", nome_arq_saida_base, size);
        }
        
        escreve_arquivo_rank0(nome_final, array_final_ordenado, n_final);
        
        libera_strings(array_final_ordenado, n_final);
        free(buffer_recebe_global);
        free(contagens_finais_bytes);
        free(desloc_finais_bytes);
    }

    free(contagens_itens);
    free(desloc_itens);
    free(contagens_finais_itens);

    MPI_Finalize();
    return 0;
}
