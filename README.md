# T1proguipar

# Objetivos
1. Implementar uma das soluções paralelas vista em sala usando o MPI para ordenação de um conjunto de sequências genômicas.
2. Comparar o desempenho com uma solução sequencial.
3. Realizar experimentos em um computador com pelo menos 8 processadores. Sua solução deveter desempenho melhor que o sequencial. A nota estará relacionada ao desempenho.
4. Elaborar um relatório descrevendo a solução, a descrição da m´áquina onde os testes foram realizados, os experimentos dos resultados obtidos bem como uma análise dos resultados


Como rodei meu código

tenho um código chamado "gerar.c" 
compilo ele com gcc -o gera gerar.c
então monto as sequências assim:
./gera 100000 100k.txt
./gera 1000000 1M.txt
./gera 10000000 10M.txt

então compilo meu SortSeq.c e SortPar.c com
gcc SortSeq.c -o seq
mpicc SortPar.c -o par

depois  executei o seq
./seq input/100k.txt saidaseq100k.txt
./seq input/1M.txt saidaseq1M.txt
./seq input/10M.txt saidaseq10M.txt
ele salva o resultado em outro arquivo :D

depois, o principal, executei assim!!!! o de paralelo
mpirun -np 2 ./par input/100k.txt saidapar100k_p2.txt
mpirun -np 4 ./par input/1M.txt saidapar1M_p4.txt
mpirun -np 8 --oversubscribe ./par input/1M.txt saidapar1M_p8.txt
do mesmo jeito do outro, só com o 8 colocando o --oversubscribe (fiz no computador da faculdade)

é isso :D

