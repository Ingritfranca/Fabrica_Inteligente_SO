#include "fabrica.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>

//Adiciona veículo e tarefa na fila
void FilaEspera::enqueue(int idVeiculo, const std::string& tarefa) {
    fila.push({idVeiculo, tarefa});
}
//Remove o primeiro veículo da fila 
std::pair<int, std::string> FilaEspera::dequeue() {
    auto front = fila.front();
    fila.pop();
    return front;
}

//Verifica se ainda existe veículo esperando
bool FilaEspera::empty() const {
    return fila.empty();
}

//Vai inicializa os robôs disponíveis
GerenciadorRobos::GerenciadorRobos(int n) {
    for (int i = 1; i <= n; ++i)
        livres.push(i);
}
 //Reserva um que esteja livre para o atendimento
int GerenciadorRobos::reservar() {
    if (livres.empty()) return -1;
    int id = livres.front();
    livres.pop();
    return id;
}

//Libera o robô para atender outro veículo
void GerenciadorRobos::liberar(int idRobo) {
    livres.push(idRobo);
}

//verifica se existe algum robô livre para atender 
bool GerenciadorRobos::temLivre() const {
    return !livres.empty();
}

// processo filho
void rodarFilho(int idRobo, int fdLeitura) {
    std::string nomeArq = "Robo_" + std::to_string(idRobo) + ".txt";
    std::ofstream log(nomeArq);
    
    bool primeiroVeiculo = true;

    FILE* pipe = fdopen(fdLeitura, "r"); //transforma o pipe em file para usr fgets
    if (!pipe) _exit(1);

    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string linha(buf);
        
        // remove \n e \r para garantir que funcione corretamente
        if (!linha.empty() && linha.back() == '\n') linha.pop_back();
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();

        if (linha.rfind("VEICULO ", 0) == 0) { //nova tarefa para um veículo
            if (!primeiroVeiculo) {
                log << "\n";
            }
            log << "Veiculo " << linha.substr(8) << ":\n";
            primeiroVeiculo = false;
        } else if (linha.rfind("TAREFA ", 0) == 0) {
            log << "- " << linha.substr(7) << "\n";
        } else if (linha == "FIM_VEICULO") {
            log.flush();
        } else if (linha == "ENCERRAR") {
            break;
        }
    }

    log.close();
    fclose(pipe);
    _exit(0);
}

//inicia sistema central, criando os processos filhos
SistemaCentral::SistemaCentral(int n, int m)
    : n(n), m(m), gerRobos(n), processos(n + 1)
{
    for (int i = 1; i <= n; ++i)
        criarProcessoFilho(i);
}

SistemaCentral::~SistemaCentral() {
    encerrarFilhos();
    aguardarFilhos();
}


void SistemaCentral::criarProcessoFilho(int idRobo) { 
    //Cria um pipe para comunicação com o processo filho
    int fds[2];  
    if (pipe(fds) == -1)
        throw std::runtime_error("Erro ao criar pipe");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("Erro ao fazer fork");

    if (pid == 0) {
        close(fds[1]);
        rodarFilho(idRobo, fds[0]);
    }

    close(fds[0]);// Pai escreve no pipe
    processos[idRobo] = {idRobo, pid, fds[1]};
}

void SistemaCentral::enviarMensagem(int idRobo, const std::string& msg) {
    std::string linha = msg + "\n";
    write(processos[idRobo].fdEscrita, linha.c_str(), linha.size());
}

void SistemaCentral::atribuirVeiculo(int idVeiculo, const std::string& tarefa, int idRobo) {
    veiculosAtivos[idVeiculo] = {idRobo, processos[idRobo].fdEscrita};
    enviarMensagem(idRobo, "VEICULO " + std::to_string(idVeiculo));// envia a identificação do veículo para o robô
    enviarMensagem(idRobo, "TAREFA " + tarefa);// envia a tarefa
}

// Trata uma nova tarefa para um veículo, alocando um robô ou colocando na fila
void SistemaCentral::handleTarefa(int idVeiculo, const std::string& tarefa) {
    auto it = veiculosAtivos.find(idVeiculo);
    if (it != veiculosAtivos.end()) {
        enviarMensagem(it->second.idRobo, "TAREFA " + tarefa);
        return;
    }
//
    auto itPendentes = tarefasPendentesFila.find(idVeiculo);
    if (itPendentes != tarefasPendentesFila.end()) {
        itPendentes->second.push_back(tarefa);
        return;
    }
//
    int idRobo = gerRobos.reservar();
    if (idRobo != -1) {
        atribuirVeiculo(idVeiculo, tarefa, idRobo);
    } else {
        tarefasPendentesFila[idVeiculo].push_back(tarefa);
        filaEspera.enqueue(idVeiculo, tarefa);
    }
}
 //trata o fim de um veículo, liberando o robô ou marcando como finalizado se estiver na fila
void SistemaCentral::handleFim(int idVeiculo) {
    auto it = veiculosAtivos.find(idVeiculo);
    if (it != veiculosAtivos.end()) {
        int idRobo = it->second.idRobo;
        enviarMensagem(idRobo, "FIM_VEICULO");
        veiculosAtivos.erase(it);
        
        gerRobos.liberar(idRobo);
        processarFila();
    } else {
        auto itPendentes = tarefasPendentesFila.find(idVeiculo);
        if (itPendentes != tarefasPendentesFila.end()) {
            veiculosFinalizadosNaFila.insert(idVeiculo);
        }
    }
}
 
void SistemaCentral::processarFila() {
    while (!filaEspera.empty() && gerRobos.temLivre()) {//
        auto [idVeiculo, primeiraTarefa] = filaEspera.dequeue();
        
        auto itPendentes = tarefasPendentesFila.find(idVeiculo);
        if (itPendentes == tarefasPendentesFila.end()) continue;

        int idRobo = gerRobos.reservar();
        if (idRobo != -1) {
            veiculosAtivos[idVeiculo] = {idRobo, processos[idRobo].fdEscrita};
            enviarMensagem(idRobo, "VEICULO " + std::to_string(idVeiculo));
            
            for (const auto& t : itPendentes->second) {
                enviarMensagem(idRobo, "TAREFA " + t);
            }
            
            tarefasPendentesFila.erase(itPendentes);
            
            if (veiculosFinalizadosNaFila.count(idVeiculo)) {
                veiculosFinalizadosNaFila.erase(idVeiculo);
                handleFim(idVeiculo); 
            }
        }
    }
}

// Envia mensagem de encerramento para os filhos e fecha os pipes
void SistemaCentral::encerrarFilhos() {
    for (int i = 1; i <= n; ++i) {
        if (processos[i].fdEscrita != -1) {
            enviarMensagem(i, "ENCERRAR");
            close(processos[i].fdEscrita);
            processos[i].fdEscrita = -1;
        }
    }
}

void SistemaCentral::aguardarFilhos() {// Espera os processos filhos terminarem
    for (int i = 1; i <= n; ++i) {
        if (processos[i].pid > 0) {
            waitpid(processos[i].pid, nullptr, 0);
            processos[i].pid = 0;
        }
    }
}

void SistemaCentral::executar() {
    std::string linha;
    while (std::getline(std::cin, linha)) {
        // Limpa possível \r vindo de arquivos do Windows
        if (!linha.empty() && linha.back() == '\r') linha.pop_back();

        if (linha == "FIM") break;
        if (linha.empty()) continue;

        // Extrai idVeiculo e tarefa usando stringstream
        std::istringstream ss(linha);
        std::string token;
        ss >> token;

        int idVeiculo;
        try { idVeiculo = std::stoi(token); }
        catch (...) { continue; }// Ignora linhas mal formatadas

        std::string resto;
        std::getline(ss, resto);
        if (!resto.empty() && resto.front() == ' ')
            resto = resto.substr(1);

        // Limpa \r do final do resto da linha, essencial para o comando "fim" ser lido certo
        if (!resto.empty() && resto.back() == '\r')
            resto.pop_back();

        if (resto == "fim")
            handleFim(idVeiculo);
        else
            handleTarefa(idVeiculo, resto);
    }

    encerrarFilhos();
    aguardarFilhos();
}