#pragma once
#include <queue>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/types.h>

// Fila de espera para veículos que aguardam robô
class FilaEspera {
public:
    void enqueue(int idVeiculo, const std::string& tarefa);
    std::pair<int, std::string> dequeue();
    bool empty() const;
private:
    std::queue<std::pair<int, std::string>> fila; 
};


class GerenciadorRobos {
public:// Inicializa os robôs disponíveis 
    explicit GerenciadorRobos(int n);
    int  reservar();  // retorna id do robô ou -1
    void liberar(int idRobo);
    bool temLivre() const;
private:
    std::queue<int> livres;
};

// Estrutura com informações de cada robô
struct ProcessoRobo {
    int   idRobo    = 0; // ID do robô (1..n)
    pid_t pid       = 0; // PID do processo filho
    int   fdEscrita = -1; // Pipe usasdo pelo paipara enviar mensagens 
};


class SistemaCentral {
public:
    SistemaCentral(int n, int m);
    ~SistemaCentral();
    void executar();

private: 
    int n, m;
    GerenciadorRobos gerRobos;
    FilaEspera       filaEspera;

    struct InfoVeiculo { int idRobo; int fdEscrita; };
    std::unordered_map<int, InfoVeiculo> veiculosAtivos; // consulta O(1)
    
    // Guarda se um veículo já está na fila esperando robô para não duplicar alocação
    std::unordered_map<int, std::vector<std::string>> tarefasPendentesFila;
    
    // Guarda os veículos que enviaram 'fim' enquanto estavam na fila
    std::unordered_set<int> veiculosFinalizadosNaFila; 

    std::vector<ProcessoRobo> processos; // índices 1..n

    void criarProcessoFilho(int idRobo);
    void enviarMensagem(int idRobo, const std::string& msg);
    void atribuirVeiculo(int idVeiculo, const std::string& tarefa, int idRobo);
    void handleTarefa(int idVeiculo, const std::string& tarefa);
    void handleFim(int idVeiculo);
    void processarFila();
    void encerrarFilhos();
    void aguardarFilhos();
};

// Função executada pelo processo filho
void rodarFilho(int idRobo, int fdLeitura);