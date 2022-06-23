#define _CRT_SECURE_NO_WARNINGS	//TOU FARTO DESTA PORCARIA


#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_PASSAGEIROS 100
#define MAX_AEROPORTOS 20
#define MAX_AVIOES 5
#define TAM 200
#define TAM_BUFFER_CIRCULAR 10		//tamanho buffer circular


#define TIMEOUT 100

//ESTADOS PARA OS AVIOES
#define ESTADO_NAO_ACEITA_NOVOS_AVIOES 0
#define ESTADO_CONECAO_INCIAL 1
#define ESTADO_AEROPORTO_INICIAL_MAL 2
#define ESTADO_ESPERA 3
#define ESTADO_EMBARCAR 4
#define ESTADO_DEFINE_DESTINO_BEM 5
#define ESTADO_DEFINE_DESTINO_MAL 6
#define ESTADO_VOO 7
#define ESTADO_DESLIGAR 8
#define ESTADO_DEFINE_AEROPORTO_INICAL 9
#define ESTADO_INDICA_NOVA_POSICAO 10


//programa controlador
#define ALREADY_CREATED_FLAG TEXT("este_valor_e_usado_para_garantir_que_so_existe_uma_instancia_a_correr_de_cada_vez")
//programa aviões
#define SEMAFORO_PARA_LIMITE_DE_CONECOES_AVIOES TEXT("SEMAFORO_PARA_LIMITE_DE_CONECOES_AVIOES")
//Aviao2Control
#define BUFFER_AVIAO2CONTROL TEXT("EVENTO_AVIAO2CONTROL")
#define MUTEX_AVIAO2CONTROL TEXT("MUTEX_AVIAO2CONTROL")
#define SEMAFORO_ESCRITA_AVIAO2CONTROL TEXT("SEMAFORO_ESCRITA_AVIAO2CONTROL")
#define SEMAFORO_LEITURA_AVIAO2CONTROL TEXT("SEMAFORO_LEITURA_AVIAO2CONTROL")
//Control2Aviao
#define BUFFER_CONTROL2AVIAO TEXT("BUFFER_CONTROL2AVIAO")
#define MUTEX_CONTROL2AVIAO TEXT("MUTEX_CONTROL2AVIAO")
#define SEMAFORO_ESCRITA_CONTROL2AVIAO TEXT("SEMAFORO_ESCRITA_CONTROL2AVIAO")
#define SEMAFORO_LEITURA_CONTROL2AVIAO TEXT("SEMAFORO_LEITURA_CONTROL2AVIAO")
//sistema de perda de aviões
#define NOME_MUTEX_TIMEOUT_PARA_AVIOES TEXT("NOME_MUTEX_TIMEOUT_PARA_AVIOES")
//aceder a variaveis locais
#define MUTEX_ACEDER_LISTA_AEROPORTOS TEXT("MUTEX_ACEDER_LISTA_AEROPORTOS")
#define MUTEX_ACEDER_LISTA_AVIOES TEXT("MUTEX_ACEDER_LISTA_AVIOES")
#define MUTEX_ACEDER_LISTA_TIMOUT_AVIOES TEXT("MUTEX_ACEDER_LISTA_TIMOUT_AVIOES")
#define MUTEX_ACEDER_LISTA_PASSAGEIROS TEXT("MUTEX_ACEDER_LISTA_PASSAGEIROS")

//estados passageiro
#define ESTADO_CONECTAR_PASSAGEIRO 1						
#define ESTADO_ARGUMENTOS_INVALIDOS 2							
#define ESTADO_ESPERA_POR_AVIAO 3
#define ESTADO_EMBARCOU_PASSAGEIRO 4
#define ESTADO_PASSAGEIRO_VOO 5
#define ESTADO_PASSAGEIRO_TIMEOUT 6
#define ESTADO_PASSAGEIRO_CHEGOU_DESTINO 7
#define ESTADO_PASSAGEIRO_QUER_DESLIGAR 8
#define ESTADO_CONTROLO_DESLIGA_PASSAGEIROS 9 
//mutex para
#define MUTEX_PARA_ACEDER_LISTA_PIPES TEXT("MUTEX_PARA_ACEDER_LISTA_PIPES")
//nome dos pipes
#define PIPE_PASSAGEIRO_2_CONTROL TEXT("\\\\.\\pipe\\PIPE_PASSAGEIRO_2_CONTROL")
#define PIPE_CONTROL_2_PASSAGEIRO TEXT("\\\\.\\pipe\\PIPE_CONTROL_2_PASSAGEIRO")


/*
	passageiro envia estado ESTADO_CONECTAR_PASSAGEIRO com nome do destino e atual. Controlo verifica se destino e atual existem
	se os aeroportos não existirem, controlo devolve ESTADO_ARGUMENTOS_INVALIDOS. Quando passageiro recebe ESTADO_ARGUMENTOS_INVALIDOS, indica argumentos mal escritos e sai
	se aeroportos estiverem bem, controlo adiciona as coordenadas corretas, adiciona o avião à lista de aviões, lança thread com este avião e devolve estado ESTADO_ESPERA_POR_AVIAO
	A thread lançada serve apenas para ler o estado ESTADO_PASSAGEIRO_TIMEOUT. Quando lê esse estado, remove o passageiro da lista de passagheioros. A comunicação do controlo->passageiro é feita na mesm thread dfos aviões
	Após receber ESTADO_ESPERA_POR_AVIAO, passageiro fica à espera o seu tempo. Após enviar ESTADO_ESPERA_POR_AVIAO thread em controlo fica à espera que este seja colocado num avião,
		No controlo, a estrutura que guarda os passageiros contem a estrutura passageiro, um idAvião e o HANDLE para o pipe de coneç~ºao controlo-passageiro
	apos este estado o programa passageiro saí e o programa controlo remove o passageiro da lista e thread desdliga-se
*/


typedef struct {
	TCHAR nome[TAM];
	int x, y;
	int estado;
	TCHAR aeroportoInicial[TAM];
	TCHAR aeroportoDestino[TAM];
	//int tempoEspera;				//se <0, Não partilhar, não é necessário, fica apenas no programa passageiro
}passageiro, pPassageiro;
typedef struct {
	passageiro passg;
	HANDLE hPipePassageiro2Control;
	HANDLE hPipeControl2Passageiro;
	int idAviao;
}blocoPassageiro, *pBlocoPassageiro;
typedef struct {
	TCHAR nome[TAM];
	int x, y;
}aeroporto, *pAeroporto;
typedef struct {
	int debug;
	int id;
	int startX, startY, destX, destY, posX, posY;
	int capacidade, numLugaresOcupados;
	TCHAR destino[TAM];
	int velocidade;
	int estado; //estado -> ESTADO_CONECAO_INCIAL |ESTADO_ESPERA | ESTADO_VOO
}aviao, *pAviao;
typedef struct {
	aviao planeBuffer[TAM_BUFFER_CIRCULAR];
	int proxEscrita;
	int proxLeitura;
	int maxPos;
	int nextID;
} AviaoEControl;
typedef struct {
	HANDLE hEvent;
	int idAviao;
	int flagAviaoDesligouse;	 //0- não faz nada. 1 - remove avião da lista e desliga-se. 2- desliga-se
} handleTimoutAviao;

void escreveNoRegistry(HKEY chave, TCHAR par_nome[], TCHAR par_valor[]);
void closeProgram(TCHAR mesage[]);
int obtemIntDoregistry(HKEY chaveRegistry, TCHAR par_nome[]);
void criarAeroporto();
void criaConeccaoComAvioes();
void verificaUnicidade();
void ObtemValoresDoRegistry();
void mostrarAeroportos();
void mostrarAvioes();
void aceitaNovosAvioes();
void escreveControl2Aviao(aviao* plane);
void removeAviaoDaLista(int id);
void enviaMensagemPipe(blocoPassageiro blocoPass);
void atualizaAviaoNaLista(aviao data);

//thread methods
DWORD WINAPI trataBufferAvioes(LPVOID lpParam);
DWORD WINAPI timeoutParaAvioes(LPVOID lpParam);
DWORD WINAPI criaConeccaoComPassageiros(LPVOID lpParam);
DWORD WINAPI conecaoComPassageiro(LPVOID lpParam);



//geral do programa
HKEY chaveRegistry = NULL;
HANDLE verificador;
//fim
boolean turnOffFlag = FALSE;
boolean aceitaAvioes = TRUE;


int max_aeroportos;		//define número máximo de aeroportos
int max_avioes;		//define número máximo de aviões

//cenas do mapa em si
pAeroporto listaAeroportos;	//lista que guarda os aeroportos
int numAeroportos;			//número de aeroportos que existem (último aeroporto criado -> listaAeroportos[numAeroportos - 1])
pAviao listaAvioes;
handleTimoutAviao* listaHandlesTimoutAvioes;
int numAvioes;
pBlocoPassageiro listaPassageiros;	//guarda passageiros
int numPassageiros;					//guarda número de passageiros



//threads
HANDLE threadConectaAvioes;

//Aviões
//atendimento dos aviões
HANDLE hLimiteAvioesConectados;		//limita número de aviões que se podem conectar
//aviao2Control
AviaoEControl* aviao2Control;			//aviao -> control
HANDLE hMutexAviao2Control;				//mutex para escrever no Aviao2Control
HANDLE hSemafotoEscritaAviao2Control;	//semaforo para avioes poderem escrever no Aviao2Control
HANDLE hSemafotoLeituraAviao2Control;	//semaforo para o Control poder ler o Aviao2Control
HANDLE hAviao2Control;					//handle para o buffer circular
//Control2Aviao
AviaoEControl* control2Aviao;			//control -> aviao
HANDLE hMutexControl2Aviao;				//mutex para escrever no Control2Aviao
HANDLE hSemafotoEscritaControl2Aviao;	//semaforo para control poder escrever no Control2Aviao
HANDLE hSemafotoLeituraControl2Aviao;	//semaforo para avioes poderem ler no Control2Aviao
HANDLE hControl2Aviao;					//handle para o buffer circular

//ACEDER A VARIAVEIS LOCAIS
HANDLE hMutexAcederAeroportos;
HANDLE hMutexAcederAvioes;
HANDLE hMutexAcederTimeoutAvioes;
HANDLE hMutexAcederPassageiros;

//Passageiros
//HANDLE* listaPassageiros;




int _tmain(int argc, TCHAR* argv[]) {
	
	
	#ifdef UNICODE
		_setmode(_fileno(stdin), _O_WTEXT);
		_setmode(_fileno(stdout), _O_WTEXT);
		_setmode(_fileno(stderr), _O_WTEXT);
	#endif

		//verifica se já existe uma instancia a correr ou não
		verificaUnicidade();

		ObtemValoresDoRegistry();
	


	_tprintf(TEXT("num max de aviões: %d\n"), max_avioes);
	_tprintf(TEXT("num max de aeroportos: %d\n"), max_aeroportos);

	listaAeroportos = malloc(max_aeroportos * sizeof(aeroporto));
	numAeroportos = 0;
	listaAvioes = malloc(max_avioes * sizeof(aviao));
	listaHandlesTimoutAvioes = malloc(max_avioes * sizeof(handleTimoutAviao));
	numAvioes = 0;

	//coloca 2 aeroportos
	wcscpy(listaAeroportos[0].nome, TEXT("lisboa"));
	listaAeroportos[0].nome[6] = '\0';
	listaAeroportos[0].x = 1;
	listaAeroportos[0].y = 1;
	numAeroportos++;
	wcscpy(listaAeroportos[1].nome, TEXT("porto"));
	listaAeroportos[1].nome[5] = '\0';
	listaAeroportos[1].x = 12;
	listaAeroportos[1].y = 12;
	numAeroportos++;

	hMutexAcederPassageiros = CreateMutex(NULL, FALSE, MUTEX_ACEDER_LISTA_PASSAGEIROS);
	hMutexAcederAeroportos = CreateMutex(NULL, FALSE, MUTEX_ACEDER_LISTA_AEROPORTOS);
	hMutexAcederAvioes = CreateMutex(NULL, FALSE, MUTEX_ACEDER_LISTA_AVIOES);
	hMutexAcederTimeoutAvioes = CreateMutex(NULL, FALSE, MUTEX_ACEDER_LISTA_TIMOUT_AVIOES);


	if (hMutexAcederAeroportos == NULL || hMutexAcederAvioes == NULL || hMutexAcederPassageiros == NULL || hMutexAcederTimeoutAvioes == NULL) {
		_tprintf(TEXT("Erro a hMutexAcederAeroportos ou hMutexAcederAvioes ou hMutexAcederTimeoutAvioes ou até hMutexAcederPassageiros -> %d"), GetLastError());
		exit(1);
	}

	//cria threads extra----------------------------------------------------------------

	//CRIA THREAD EVENTO E TUDO PARA CONEÇÃO COM OS AVIÕES
	criaConeccaoComAvioes();
	
	HANDLE threadWaitConecaoPassageiros = CreateThread(NULL, 0, criaConeccaoComPassageiros, NULL, 0, NULL);
	if (threadWaitConecaoPassageiros == NULL) {
		_tprintf(TEXT("\nErro a criar thread que fica à espera da coneção dos passageiros -> %d\n"), GetLastError());
		exit(1);
	}
	

	//loop para criar aeroportos, mostrar cenas e desligar
	do {
		TCHAR input[TAM];

		_tprintf(TEXT("\n\n\nnovo aero - cria novo aeroporto\n"));
		_tprintf(TEXT("lista aero - lista aeroportos\n"));
		_tprintf(TEXT("lista aviao - lista aviões\n"));
		_tprintf(TEXT("aceita aviao - mudar o aceita aviões\n"));
		_tprintf(TEXT("'fim' para sair\n\n"));

		_fgetts(input, TAM - 1, stdin);
		input[_tcslen(input) - 1] = '\0';

		if (0 == _tcscmp(TEXT("fim"), input)) {
			break;
		}
		else if (0 == _tcscmp(TEXT("novo aero"), input)) {
			criarAeroporto();
		}
		else if (0 == _tcscmp(TEXT("lista aero"), input)) {
			mostrarAeroportos();
		}
		else if (0 == _tcscmp(TEXT("lista aviao"), input)) {
			mostrarAvioes();
		}
		else if (0 == _tcscmp(TEXT("aceita aviao"), input)) {
			aceitaNovosAvioes();
		}



	} while (1);
	turnOffFlag = TRUE;

	//Manda mensagem para desligar os passageiros
	WaitForSingleObject(hMutexAcederPassageiros, INFINITE);

	for (int i = 0; i < numPassageiros; i++) {
		listaPassageiros[i].passg.estado = ESTADO_CONTROLO_DESLIGA_PASSAGEIROS;
		enviaMensagemPipe(listaPassageiros[i]);
	}

	ReleaseMutex(hMutexAcederPassageiros);

	//ESPERA QUE OS AVIÕES DESLIGUEM
	WaitForSingleObject(threadConectaAvioes, INFINITE);
	CloseHandle(threadConectaAvioes);

	CloseHandle(hMutexAcederTimeoutAvioes);
	CloseHandle(verificador);	//elimina a handle para que permita que o controlador possa ser ligado multiplas vezes
	return 0;
}


//verificação inicial do registry
void escreveNoRegistry(HANDLE chave, TCHAR par_nome[], TCHAR par_valor[]) {
	//adicionar par nome-valor
	if (RegSetValueEx(chave, par_nome, 0, REG_SZ, (LPBYTE)par_valor, sizeof(TCHAR) * (_tcslen(par_valor) + 1)) == ERROR_SUCCESS)
		_tprintf(TEXT("Par chave-valor com nome <%s> e valor <%s> escrita com sucesso no registry\n"), par_nome, par_valor);
	else
	{
		TCHAR mesage[TAM];
		_stprintf_s(mesage, TAM, TEXT("Falha a escrever no resgistry chave-valor com nome <%s> e valor <%s>\n"), par_nome, par_valor);
		closeProgram(mesage);
	}
}
int obtemIntDoregistry(HKEY chaveRegistry, TCHAR par_nome[]) {
	TCHAR value_obtained[TAM];
	DWORD size = TAM;
	//consultar par nome-valor
	if (RegQueryValueEx(chaveRegistry, par_nome, NULL, NULL, (LPBYTE)value_obtained, &size) == ERROR_SUCCESS)
		return _tstoi(value_obtained);
		
	return -1;
}
void ObtemValoresDoRegistry() {

	TCHAR par_nome[TAM], par_valor[TAM];
	TCHAR chave_avioes[] = TEXT("Software\\SO2\\TPMDMF\\AVIOES\\");
	TCHAR chave_aeroportos[] = TEXT("Software\\SO2\\TPMDMF\\AEROPORTOS\\");


	//Obtem valores para aviões
	if (RegCreateKeyEx(HKEY_CURRENT_USER, chave_avioes, 0, NULL, REG_OPTION_VOLATILE, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &chaveRegistry, NULL) != ERROR_SUCCESS)
	{
		_tprintf(TEXT("\nErro critico a ler chave de aviões!\nA desligar programa\n"));
		exit(-1);
	}
	//tenta obter
	wcscpy_s(par_nome, TAM, TEXT("MAXAVIOES"));
	max_avioes = obtemIntDoregistry(chaveRegistry, par_nome);
	if (max_avioes == -1)	//caso não exista para obter, escreve
	{
		wcscpy_s(par_nome, TAM, TEXT("MAXAVIOES"));
		_stprintf_s(par_valor, TAM, TEXT("%d"), MAX_AVIOES);
		escreveNoRegistry(chaveRegistry, par_nome, par_valor);
		max_avioes = MAX_AVIOES;
	}


	//fecha a handle de aviões
	CloseHandle(chaveRegistry);

	if (RegCreateKeyEx(HKEY_CURRENT_USER, chave_aeroportos, 0, NULL, REG_OPTION_VOLATILE, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &chaveRegistry, NULL) != ERROR_SUCCESS) {
		_tprintf(TEXT("\nErro critico a ler a chave do controlador2!\nA desligar o programa\n"));
		exit(-1);
	}
	//tenta obter
	wcscpy_s(par_nome, TAM, TEXT("MAXAEROPORTOS"));
	max_aeroportos = obtemIntDoregistry(chaveRegistry, par_nome);
	if (max_aeroportos == -1)//caso não exista para obter, escreve 
	{
		wcscpy_s(par_nome, TAM, TEXT("MAXAEROPORTOS"));
		_stprintf_s(par_valor, TAM, TEXT("%d"), MAX_AEROPORTOS);
		escreveNoRegistry(chaveRegistry, par_nome, par_valor);
		max_aeroportos = MAX_AEROPORTOS;
	}

	//fecha a handle de aeroportos
	CloseHandle(chaveRegistry);

}

//UI Methods
void criarAeroporto() {

	if (numAeroportos >= max_aeroportos)
	{
		_tprintf(TEXT("\nNão é possivel criar mais aeroportos!\n"));
		return;
	}
	TCHAR nome[TAM];
	boolean flag =	FALSE;

	//obtem nome
	while (!flag) {
		flag = TRUE;
		_tprintf(TEXT("indique nome do aeroporto (sem espaço):\n"));
		//_tscanf_s(TEXT("%s"), &nome, TAM);
		_fgetts(nome, TAM - 1, stdin);
		nome[_tcslen(nome) - 1] = '\0';

		for (int i = 0; i < numAeroportos; i++) {
			if (0 == _tcscmp(nome, listaAeroportos[i].nome)){
				_tprintf(TEXT("\nEste nome <%s> já existe, coloque outro\n"), nome);
				flag = FALSE;
			}
		}
	}

	//obtem coordenadas
	flag = FALSE;
	int x, y;
	while (!flag) {
		flag = TRUE;
		_tprintf(TEXT("\nindique posição X (0 - 999):\n"));
		_tscanf_s(TEXT("%d"), &x);

		_tprintf(TEXT("\nindique posição Y (0 - 999):\n"));
		_tscanf_s(TEXT("%d"), &y);

		if (x > 999 || x < 0){
			_tprintf(TEXT("\nValor de X inválido\n"));
			flag = FALSE;
		}
		else if (y > 999 || y < 0) {
			_tprintf(TEXT("\nValor de Y inválido\n"));
			flag = FALSE;
		}
		else
			for (int i = 0; i < numAeroportos; i++) {
				long distancia = sqrt(pow(((double) listaAeroportos[i].x - (double)x), 2) + pow(((double)listaAeroportos[i].y - (double)y), 2));	//distancia entre dois pontos
				if (distancia < 10) {
					_tprintf(TEXT("\nCoordenadas do aeroporto a menos de 10 casas do aeroporto mais próximo\n"));
					flag = FALSE;
				}
			}
	}

	//coloca os valores na posição devida
	wcscpy_s(listaAeroportos[numAeroportos].nome,TAM, nome);
	listaAeroportos[numAeroportos].x = x;
	listaAeroportos[numAeroportos].y = y;
	
	//Aumenta o counter
	WaitForSingleObject(hMutexAcederAeroportos, INFINITE);
	numAeroportos++;									
	ReleaseMutex(hMutexAcederAeroportos);


}
void mostrarAeroportos() {
	_tprintf(TEXT("\nLista de aeroportos:\n"));

	WaitForSingleObject(hMutexAcederAeroportos, INFINITE);

	for (int i = 0; i < numAeroportos; i++) {
		_tprintf(TEXT("nome: %s\tCordenadas: { %d ; %d }\n"), listaAeroportos[i].nome, listaAeroportos[i].x, listaAeroportos[i].y);
	}

	ReleaseMutex(hMutexAcederAeroportos);
	_tprintf(TEXT("\n"));
}
void mostrarAvioes() {
	_tprintf(TEXT("\nLista de aviões:\n"));
	WaitForSingleObject(hMutexAcederAvioes, INFINITE);
	for (int i = 0; i < numAvioes; i++) {
		_tprintf(TEXT("id avião: %d\tCoordenadas { %d ; %d }\tDestino: %s\n"), listaAvioes[i].id, listaAvioes[i].posX, listaAvioes[i].posY, listaAvioes[i].destino);
	}

	ReleaseMutex(hMutexAcederAvioes);
	_tprintf(TEXT("\n"));
}
void aceitaNovosAvioes() {
	aceitaAvioes = !aceitaAvioes;
	if (aceitaAvioes) {
		_tprintf(TEXT("\nO programa está agora a aceitar novos aviões!\n"));
	}
	else
		_tprintf(TEXT("\nO programa não esta a aceitar novos aviões!\n"));
}

//Generalist Error Methods
void closeProgram(TCHAR mesage[]) {
	if (chaveRegistry != NULL && RegCloseKey(chaveRegistry) == ERROR_SUCCESS)
		_tprintf(TEXT("\nErro crítico! Mensagem de erro:\t%s\nA sair...\n"), mesage);
	else
		_tprintf(TEXT("\nErro a fechar chave de registo!\n\nA DESLIGAR NA MESMA!\n"));

	CloseHandle(verificador);	//elimina a handle para que permita que o controlador possa ser ligado multiplas vezes
			
	exit(0);
}
void verificaUnicidade() {

	verificador = CreateFileMappingA(
		INVALID_HANDLE_VALUE,		//usado para quando não há um ficheiro a abrir, é só na ram
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(TCHAR),
		ALREADY_CREATED_FLAG
	);
	if (verificador == NULL) {
		_tprintf(TEXT("Não foi possivel verificar se existia outra instancia a correr\nA desligar\n"));
		return;
	}
	else if (GetLastError() == ERROR_ALREADY_EXISTS) {
		_tprintf(TEXT("Já se encontra ligada uma instancia do controlador\nA desligar\n"));
		return;
	}

}

//Aviões
void criaConeccaoComAvioes() {
	//LIMITADOR DE CONEÇÕES 
	hLimiteAvioesConectados = CreateSemaphore(NULL, max_avioes, max_avioes, SEMAFORO_PARA_LIMITE_DE_CONECOES_AVIOES);
	if (hLimiteAvioesConectados == NULL) {
		_tprintf(TEXT("Erro a crear semaforo limitador de coneçoes-> %d\n"), GetLastError());
		exit(1);
	}


	/* CONTROL2AVIAO
	AviaoEControl* control2Aviao;			//control -> aviao
	HANDLE hMutexControl2Aviao;				//mutex para escrever no Control2Aviao
	HANDLE hSemafotoEscritaControl2Aviao;	//semaforo para control poder escrever no Control2Aviao
	HANDLE hSemafotoLeituraControl2Aviao;	//semaforo para avioes poderem ler no Control2Aviao
	HANDLE hControl2Aviao;					//handle para o buffer circular
	*/
	hMutexControl2Aviao = CreateMutex(NULL, FALSE, MUTEX_CONTROL2AVIAO);
	hSemafotoEscritaControl2Aviao = CreateSemaphore(NULL, TAM_BUFFER_CIRCULAR, TAM_BUFFER_CIRCULAR, SEMAFORO_ESCRITA_CONTROL2AVIAO);
	hSemafotoLeituraControl2Aviao = CreateSemaphore(NULL, 0, TAM_BUFFER_CIRCULAR, SEMAFORO_LEITURA_CONTROL2AVIAO);

	if (hMutexControl2Aviao == NULL || hSemafotoEscritaControl2Aviao == NULL || hSemafotoLeituraControl2Aviao == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex -> %d\n"), GetLastError());
		exit(1);
	}

	hControl2Aviao = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(AviaoEControl), //tamanho da memoria partilhada
		BUFFER_CONTROL2AVIAO);//nome do filemapping. nome que vai ser usado para partilha entre processos

	if (hControl2Aviao == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping -> %d\n"), GetLastError());
		exit(1);
	}

	control2Aviao = (AviaoEControl*)MapViewOfFile(hControl2Aviao, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (control2Aviao == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile -> %d\n"), GetLastError());
		exit(1);
	}

	control2Aviao->proxEscrita = 0;
	control2Aviao->proxLeitura = 0;
	control2Aviao->maxPos = TAM_BUFFER_CIRCULAR;
	control2Aviao->nextID = 0;	



	/*	AVIAO 2 CONTROL
	AviaoEControl* aviao2Control;			//aviao -> control
	HANDLE hMutexAviao2Control;				//mutex para escrever no Aviao2Control
	HANDLE hSemafotoEscritaAviao2Control;	//semaforo para avioes poderem escrever no Aviao2Control
	HANDLE hSemafotoLeituraAviao2Control;	//semaforo para o Control poder ler o Aviao2Control
	HANDLE hAviao2Control;					//handle para o buffer circular
	*/

	hMutexAviao2Control = CreateMutex(NULL, FALSE, MUTEX_AVIAO2CONTROL);
	hSemafotoEscritaAviao2Control = CreateSemaphore(NULL, TAM_BUFFER_CIRCULAR, TAM_BUFFER_CIRCULAR, SEMAFORO_ESCRITA_AVIAO2CONTROL);
	hSemafotoLeituraAviao2Control = CreateSemaphore(NULL, 0, TAM_BUFFER_CIRCULAR, SEMAFORO_LEITURA_AVIAO2CONTROL);

	if (hMutexAviao2Control == NULL || hSemafotoEscritaAviao2Control == NULL || hSemafotoLeituraAviao2Control == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex -> %d\n"), GetLastError());
		exit(1);
	}

	//criamos o bloco de memoria partilhada
	hAviao2Control = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(AviaoEControl), //tamanho da memoria partilhada
		BUFFER_AVIAO2CONTROL);//nome do filemapping. nome que vai ser usado para partilha entre processos

	if (hAviao2Control == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		exit(1);
	}


	aviao2Control = (AviaoEControl*)MapViewOfFile(hAviao2Control, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (aviao2Control == NULL) {
		_tprintf(TEXT("\nErro a mapear a memoria (MapViewOfFile) -> %d\n"), GetLastError());
		exit(1);
	}

	aviao2Control->proxEscrita = 0;
	aviao2Control->proxLeitura = 0;
	aviao2Control->maxPos = TAM_BUFFER_CIRCULAR;


	//lança  a thread que comunica com os avioes
	threadConectaAvioes = CreateThread(NULL, 0, trataBufferAvioes, NULL, 0, NULL);
	if (threadConectaAvioes == NULL) {
		_tprintf(TEXT("Erro a criar thread para o buffer circular-> %d\n"), GetLastError());
		exit(-99);
	}

}

/*

#define ESTADO_NAO_ACEITA_NOVOS_AVIOES 0
#define ESTADO_CONECAO_INCIAL 1
#define ESTADO_AEROPORTO_INICIAL_MAL 2
#define ESTADO_ESPERA 3
#define ESTADO_EMBARCAR 4
#define ESTADO_DEFINE_DESTINO_BEM 5
#define ESTADO_DEFINE_DESTINO_MAL 6
#define ESTADO_VOO 7
#define ESTADO_DESLIGAR 8
#define ESTADO_DEFINE_AEROPORTO_INICAL 9

*
*
Esta thread usa 2 memorias partilhadas
Aviao2Control -> onde os aviões escrevem a sua info
Control2Aviao -> onde o control dá a resposta aos aviões

Os aviões conectam-se as mesmas, sendo que so escrevem na Aviao2Control -> o avião é quem escreve o id
Quando se conectam colocam o estado ESTADO_CONECAO_INCIAL e o nome do aeroporto inicial no 'destino'  <----------------- feito acima
O Control testa se o estado for ESTADO_CONECAO_INCIAL , se o destino esta bem escrito e se aceita aviões
Se não aceitar aviões, escreve no buffer Control2Aviao com estado ESTADO_NAO_ACEITA_NOVOS_AVIOES. Ao ler esse estado o avião sai.
Caso o nome do aerporto estiver mal escreve no Control2Aviao Com o estado ESTADO_AEROPORTO_INICIAL_MAL.
Caso esteja tudo bem devolve ESTADO_ESPERA

Quando o aviao recebe ESTADO_AEROPORTO_INICIAL_MAL pede um novo nome de aeroporto e devolve ESTADO_DEFINE_AEROPORTO_INICAL
Quando o controlador recebe ESTADO_DEFINE_AEROPORTO_INICAL, verifica se existe um aeroporto com esse nome. Se sim, coloca coordenadas inicias e devolve ESTADO_ESPERA.
Se nao existir, devolve ESTADO_AEROPORTO_INICIAL_MAL

Quando o control recebe um aviao com ESTADO_DEFINE_DESTINO_BEM verifica se o destino existe. Se existir devoolve ESTADO_DEFINE_DESTINO_BEM. Se não existir, devolve ESTADO_DEFINE_DESTINO_MAL
Quando um aviao receve ESTADO_DEFINE_DESTINO_MAL obtem um novo destino do utilizador e escreve-o com ESTADO_DEFINE_DESTINO_BEM
Quando um aviao recebe ESTADO_DEFINE_DESTINO_BEM decide ou embaracar pessoas, enviando ESTADO_EMBARCAR ou entao comeczar viagem devolvendo ESTADO_VOO e a posição seguinte de viagem.

Quando o controlador recebe ESTADO_EMBARCAR embarca as pessoas para o avião e devolve com ESTADO_EMBARCAR
Quando o aviao recebe ESTADO_EMBARCAR tem de iniciar viagem, usando ESTADO_VOO e a posição seguinte

Quando o Control recebe ESTADO_VOO verifica se a posição esta livre. Se estiver atualiza os dados na sua lista, se não, não os atualiza e devolve o que tem na sua lista.
Se o Controlo detetar que chegou ao aeroporto de destino desembarca toda a gente e devolve ESTADO_ESPERA
Quando o aviao recebe ESTADO_VOO calcula a proxima posição e devolve ESTADO_VOO

O aviao le o ESTADO_ESPERA e tem de definir para onde ir (obtem destino e envia ESTADO_DEFINE_DESTINO_BEM) OU sair, de onde devolve ESTADO_DESLIGAR e desliga o programa
Quando aviao recebe ESTADO_DESLIGAR, desliga o proghrama por ordem do control
Quando control le ESTADO_DESLIGAR, remove o aviao da lista

*/
DWORD WINAPI trataBufferAvioes(LPVOID lpParam) {
	//espera ter lugar no semaforo para escrita
	//coloca dados iniciais no buffer circular:

	while (!turnOffFlag) {
		aviao data;
		//espera para poder ler os aviões do buffer
		WaitForSingleObject(hSemafotoLeituraAviao2Control ,INFINITE);	//espera semaforo
		WaitForSingleObject(hMutexAviao2Control, INFINITE);				//espera ter acesso à info de maneira segura
		//_tprintf(TEXT("\nLeu em: %d"), aviao2Control->proxLeitura);
		memcpy(&data, &aviao2Control->planeBuffer[aviao2Control->proxLeitura], sizeof(data));	//copia a informação para si
		aviao2Control->proxLeitura == (aviao2Control->maxPos - 1) ? aviao2Control->proxLeitura = 0 : aviao2Control->proxLeitura++;	//aumenta contador da proxima leitura


		ReleaseMutex(hMutexAviao2Control);								//liberta o mutex
		if (ReleaseSemaphore(hSemafotoEscritaAviao2Control, 1, NULL) == 0)		//indica que pode escrever mais um!
			_tprintf(TEXT("Erro a libertar semáforo hSemafotoEscritaAviao2Control -> %d\n"), GetLastError());

		//verifica se esta na lista e, se estiver, manda o evento!!!
		if (data.estado != ESTADO_CONECAO_INCIAL) {
			//verifica se o avião existe no array de aviões
			boolean flagExisteNaLista = FALSE;
			WaitForSingleObject(hMutexAcederAvioes, INFINITE);
			for (int i = 0; i < numAvioes; i++) {
				if (data.id == listaAvioes[i].id) {
					flagExisteNaLista = TRUE;
					break;
				}
			}
			ReleaseMutex(hMutexAcederAvioes);
			if (!flagExisteNaLista)	//se não existir "ignora"
			{
				_tprintf(TEXT("Aviao não esta na lista\t;)\n"));
				continue;
			}
			//se existir, sinaliza o evento!
			WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
			for (int i = 0; i < numAvioes; i++) 
				if (listaHandlesTimoutAvioes[i].idAviao == data.id) {
					SetEvent(listaHandlesTimoutAvioes[i].hEvent);
					break;
				}		
			ReleaseMutex(hMutexAcederTimeoutAvioes);
		}


		//O Control testa se o estado for ESTADO_CONECAO_INCIAL, se o destino esta bem escrito e se aceita aviões
		//Se não aceitar aviões, escreve no buffer Control2Aviao com estado ESTADO_NAO_ACEITA_NOVOS_AVIOES.Ao ler esse estado o avião sai e NAO guarda o aviao na lista.
		//Caso o nome do aerporto estiver mal escreve no Control2Aviao Com o estado ESTADO_AEROPORTO_INICIAL_MAL e guarda o avião na lista.
		//Caso esteja tudo bem devolve ESTADO_ESPERA e guarda o avião na lista .
		if (data.estado == ESTADO_CONECAO_INCIAL) {

			if (!aceitaAvioes) {		//caso nao aceite aviões
				data.estado = ESTADO_NAO_ACEITA_NOVOS_AVIOES;

				escreveControl2Aviao(&data);
				continue;	//avança para ler o seguinte
			}

			_tprintf(TEXT("Um avião com id %d conectou-se\n"), data.id);

			//verifica se o aeroporto inicial é o correto
			boolean flagAerportoExiste = FALSE;
			WaitForSingleObject(hMutexAcederAeroportos, INFINITE);		//pede mutex para aceder aos aeroportos
			for (int i = 0; i < numAeroportos; i++) {
				if (wcscmp(listaAeroportos[i].nome, data.destino)==0) {
					flagAerportoExiste = TRUE;
					data.startX = listaAeroportos[i].x;
					data.startY = listaAeroportos[i].y;
					data.posX = listaAeroportos[i].x;
					data.posY = listaAeroportos[i].y;

					break;
				}
			}
			ReleaseMutex(hMutexAcederAeroportos);						//liberta o mutex para aceder aos aeroporto


			//caso o aeroporto exista, tem de colocar aeroporto na lista
			if (!flagAerportoExiste) {
				_tprintf(TEXT("\nAvião com id = %d colocou uma origem inválida-> %s\n"), data.id, data.destino);
				data.estado = ESTADO_AEROPORTO_INICIAL_MAL;
			}
			else
			{
				_tprintf(TEXT("\nAvião com id = %d colocou uma origem existente\n"), data.id);
				wcscpy(&data.destino, TEXT("not defined by user yet"));
				data.estado = ESTADO_ESPERA;
	
				//cria thread que faz adiciona aviao à lista:
				
				HANDLE novaThreadTimeout = CreateThread(NULL, 0, &timeoutParaAvioes, &data, 0, NULL);
				if (novaThreadTimeout == NULL) {
					_tprintf(TEXT("Erro a criar thread para o buffer circular-> %d\n"), GetLastError());
					exit(1);
				}
				
			}
			
			

			
			

			//envia no final
			escreveControl2Aviao(&data);

		}
		//Quando o controlador recebe ESTADO_DEFINE_AEROPORTO_INICAL, verifica se existe um aeroporto com esse nome. Se sim, coloca coordenadas inicias e devolve ESTADO_ESPERAe atualiza cordenadas iniciais e posisição na lista.
		//Se nao existir, devolve ESTADO_AEROPORTO_INICIAL_MAL
		else if (data.estado == ESTADO_DEFINE_AEROPORTO_INICAL) {
			_tprintf(TEXT("\nAvião com id = %d indica que tem origem em %s\n"), data.id, data.destino);


			//verifica se o aeroporto inicial é o correto
			boolean flagAerportoExiste = FALSE;
			WaitForSingleObject(hMutexAcederAeroportos, INFINITE);		//pede mutex para aceder aos aeroportos
			for (int i = 0; i < numAeroportos; i++) {
				if (wcscmp(listaAeroportos[i].nome, data.destino) == 0) {
					flagAerportoExiste = TRUE;
					data.startX = listaAeroportos[i].x;
					data.startY = listaAeroportos[i].y;
					data.posX = listaAeroportos[i].x;
					data.posY = listaAeroportos[i].y;

					break;
				}
			}

			ReleaseMutex(hMutexAcederAeroportos);						//liberta o mutex para aceder aos aeroportos
			//caso o aeroporto exista, tem de colocar aeroporto s
			if (flagAerportoExiste) {
				_tprintf(TEXT("\nAvião com id = %d colocou uma origem válida\n"), data.id);

				data.estado = ESTADO_ESPERA;
			}
			else
			{
				_tprintf(TEXT("\nAvião com id = %d colocou uma origem inválida -> %s\n"), data.id, data.destino);

				data.estado = ESTADO_AEROPORTO_INICIAL_MAL;
			}

			escreveControl2Aviao(&data);

			//atualiza o aviao da lista:
			atualizaAviaoNaLista(data);
		}
		//Quando o control recebe um aviao com ESTADO_DEFINE_DESTINO_BEM verifica se o destino existe. Se existir devoolve ESTADO_DEFINE_DESTINO_BEM. Se não existir, devolve ESTADO_DEFINE_DESTINO_MAL
		else if (data.estado == ESTADO_DEFINE_DESTINO_BEM) {
			_tprintf(TEXT("\nAvião com id = %d esta a verificar se o destino %s existe\n"), data.id, data.destino);

			//ve se o aeroporto existe
			boolean flagAerportoExiste = FALSE;
			WaitForSingleObject(hMutexAcederAeroportos, INFINITE);		//pede mutex para aceder aos aeroportos
			for (int i = 0; i < numAeroportos; i++) {
				if (wcscmp(listaAeroportos[i].nome, data.destino) == 0) {
					flagAerportoExiste = TRUE;
					data.destX = listaAeroportos[i].x;
					data.destY = listaAeroportos[i].y;
					break;
				}
			}
			ReleaseMutex(hMutexAcederAeroportos);						//liberta o mutex para aceder aos aeroportos

			//caso o aeroporto exista, atualiza dados de destino
			if (flagAerportoExiste) {
				//atualiza os dados na lista de aviões
				atualizaAviaoNaLista(data);

				_tprintf(TEXT("\nAvião com id = %d teve o seu destino aprovado\n"), data.id);

				//muda o estado
				data.estado = ESTADO_DEFINE_DESTINO_BEM;
			}
			else
			{
				_tprintf(TEXT("\nAvião com id = %d colocou um destino inválido!\n"), data.id);

				data.estado = ESTADO_DEFINE_DESTINO_MAL;	
			}

			//escreve o aviao no Control2Aviao
			escreveControl2Aviao(&data);


		}
		//Quando o controlador recebe ESTADO_EMBARCAR embarca as pessoas para o avião e devolve com ESTADO_EMBARCAR
		else if (data.estado == ESTADO_EMBARCAR) {	
			WaitForSingleObject(hMutexAcederPassageiros, INFINITE);
			//procura pelo passageiro na lista
			for (int i = 0; i < numPassageiros; i++) {
				//verifica se tem capacidade para mais passageiros
				if (data.numLugaresOcupados == data.capacidade)
					break;
				//quando encontra passageiro em espera
				if (listaPassageiros[i].idAviao == -1)
					if (wcscmp(listaPassageiros[i].passg.aeroportoDestino, data.destino) == 0)	//aviao com o mesmo destino
						if (listaPassageiros[i].passg.x == data.posX && listaPassageiros[i].passg.y == data.posY) {	//estão na mesma posição (mesmo aeroporto incial)
							//coloca o id do avião no passageiro
							listaPassageiros[i].idAviao = data.id;
							//muda estado
							listaPassageiros[i].passg.estado = ESTADO_EMBARCOU_PASSAGEIRO;
							//envia mensagem para o programa passageiro
							enviaMensagemPipe(listaPassageiros[i]);
							++data.numLugaresOcupados;
							_tprintf(TEXT("\nPessoa nome <%s> embarcou no avião"), listaPassageiros[i].passg.nome);
						}
			}
			ReleaseMutex(hMutexAcederPassageiros);

			atualizaAviaoNaLista(data);

			escreveControl2Aviao(&data);
		}
		//Quando o Control recebe ESTADO_VOO verifica se a posição esta livre. Se estiver atualiza os dados na sua lista, se não, não os atualiza e devolve o que tem na sua lista.
		//Se o Controlo detetar que chegou ao aeroporto de destino desembarca toda a gente e devolve ESTADO_ESPERA
		else if (data.estado == ESTADO_VOO) {
	
			//verifica se chegou a aeroporto
			boolean chegouAeroportoFlag = FALSE;
			if (data.posX == data.destX && data.posY == data.destY) {
				chegouAeroportoFlag = TRUE;
			}
			ReleaseMutex(hMutexAcederAeroportos);
			//----------------------------------
			//caso tenha chegado a um aeroporto!
			//----------------------------------
			if (chegouAeroportoFlag) {
				_tprintf(TEXT("Aviao id = %d chegou a aeroporto\t A desembarcar pessoas\n"), data.id);
				//desembarcar pessoas
				WaitForSingleObject(hMutexAcederPassageiros, INFINITE);
				for (int i = 0; i < numPassageiros; i++) {
					if (listaPassageiros[i].idAviao == data.id)
					{
						//atualiza coordenadas
						//NÃO é preciso!
						//muda o estado
						listaPassageiros[i].passg.estado = ESTADO_PASSAGEIRO_CHEGOU_DESTINO;
						//envia mensagem
						enviaMensagemPipe(listaPassageiros[i]);
						//o passageiro é removido quando voltar a receber a mensagem no pipe
					}
				}
				ReleaseMutex(hMutexAcederPassageiros);

				//mudar estado
				data.estado = ESTADO_ESPERA;
				data.capacidade = 0;
				atualizaAviaoNaLista(data);

			}
			//não chegou a aeroporto!
			else {

				boolean flagPosicaoUsada = FALSE;
				//verifica se a casa esta livre
				WaitForSingleObject(hMutexAcederAvioes, INFINITE);	
				for (int i = 0; i < numAvioes; i++) {
					if (data.posX == listaAvioes[i].posX && data.posY == listaAvioes[i].posY) {
						flagPosicaoUsada = TRUE;					//alguem esta naquela posição
						break;
					}
				}
				ReleaseMutex(hMutexAcederAvioes);

				//caso a posição esteja livre -> atualiza na propria lista
				if (!flagPosicaoUsada) {	
					//atualiza passageiros
					WaitForSingleObject(hMutexAcederPassageiros, INFINITE);
					for (int i = 0; i < numPassageiros; i++) {
						if (listaPassageiros[i].idAviao == data.id)
						{
							//atualiza coordenadas
							//NÃO é preciso!
							//muda o estado
							listaPassageiros[i].passg.estado = ESTADO_PASSAGEIRO_VOO;
							listaPassageiros[i].passg.x = data.posX;
							listaPassageiros[i].passg.y = data.posY;
							//envia mensagem
							enviaMensagemPipe(listaPassageiros[i]);
							//o passageiro é removido quando voltar a receber a mensagem no pipe
						}
					}
					ReleaseMutex(hMutexAcederPassageiros);


					//Atualiza avião na lista
					atualizaAviaoNaLista(data);
					_tprintf(TEXT("O avião id = %d esta em voo! { %d ; %d }\n"), data.id, data.posX, data.posY);
				}
				else {
					//NÃO atualiza avião na lista!
					_tprintf(TEXT("O avião id = %d cometeu um erro na leitura de coordenadas, espera-se uma correção!\n"), data.id);
					data.estado = ESTADO_INDICA_NOVA_POSICAO;
				}
			}
			//escreve o aviao no Control2Aviao
			escreveControl2Aviao(&data);

		}
		else if (data.estado == ESTADO_DESLIGAR) {
				
			WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
			for (int i = 0; i < numAvioes; i++) {
				if (listaHandlesTimoutAvioes[i].idAviao == data.id) {
					listaHandlesTimoutAvioes[i].flagAviaoDesligouse = 1;
					SetEvent(listaHandlesTimoutAvioes[i].hEvent);					//lança o evento para vir limpar o avião
				}
			}
			ReleaseMutex(hMutexAcederTimeoutAvioes);
			_tprintf(TEXT("\nAvião com id = %d desligou-se\n"), data.id);
		}
	}
	
	//desligar todos os aviões!
	aceitaAvioes = FALSE;		//deliga logo isto para evitar possiveis bugs casuais!

	WaitForSingleObject(hMutexAcederAvioes, INFINITE);
	WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
	//manda mensagem a todos para sairem!
	for (int i = 0; i < numAvioes; i++) {
		
		listaAvioes[i].estado = ESTADO_DESLIGAR;
		escreveControl2Aviao(&listaAvioes[i]);

		listaHandlesTimoutAvioes[i].flagAviaoDesligouse = 2;
		SetEvent(listaHandlesTimoutAvioes[i].hEvent);
	}
	ReleaseMutex(hMutexAcederAvioes);
	ReleaseMutex(hMutexAcederTimeoutAvioes);
	return 0;
}

DWORD WINAPI timeoutParaAvioes(LPVOID lpParam) {
	/*
		recebe o avião
		lanca uma thread destas por cada avião, quando os aviões se conectam e são aceites
		Quando os aviões são removidos (sem ser por aqui), muda uma flag na estrutura e verifica-a
		Se tiver saido por timeout, então remove o avião da lista de aviões, fecha a handle e remove a estrutura da lista de handles e aviões.
		Se a flag indicar que avião for removido, desliga-se.
		Se a flag não indicar que o avião foi removido, faz o auto reset e volta a ficar à espera
		#####################################
		QUEM REMOVE OS AVIÕES DA LISTA É ESTA THREAD, NÃO A DE COMUNICAÇÃO!!!!!!!!!!!!!!!!!!!! <-para manter sincronização
	*/

	aviao* plane = malloc(sizeof(aviao));
	if (plane == NULL)
	{
		_tprintf(TEXT("\nMalloc não consegiu alocar plane para timeout -> %d\n"), GetLastError());
		exit(1);
	}
	memcpy(plane, (aviao*)lpParam, sizeof(aviao));	//copia para novo endereço, assim não é modificado externamente!
	_tprintf(TEXT("novo avião com id %d\n"), plane->id);

	
	//cria evento bloqueado
	HANDLE hAviao = CreateEvent(NULL, FALSE, FALSE, NULL);	//O EVENTO RESETA SOZINHO. Não precisa de nome porque não é acedido de nenhum outro lado
	if (hAviao == NULL) {
		_tprintf(TEXT("\nErro a criar evento para timeout de um avião...\nA sair\n"));
		exit(1);
	}


	//coloca(guarda no programa) o avião na lista
	WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
	WaitForSingleObject(hMutexAcederAvioes, INFINITE);

	memcpy(&listaAvioes[numAvioes], plane, sizeof(*plane));

	//coloca avião na lista
	listaHandlesTimoutAvioes[numAvioes - 1].flagAviaoDesligouse = 0;
	listaHandlesTimoutAvioes[numAvioes].idAviao = plane->id;
	listaHandlesTimoutAvioes[numAvioes].hEvent = hAviao;
	numAvioes++;

	ReleaseMutex(hMutexAcederAvioes);
	ReleaseMutex(hMutexAcederTimeoutAvioes);


	//fica em loop a espera de desbloquear ou TIMEOUT segundos
	while(1){
		//ResetEvent(hAviao);

		//############################################################
		//EVENTO DE ESPERA DE TIMEOUT
		DWORD resp = WaitForSingleObject(hAviao, TIMEOUT * 1000);
		//############################################################

		//verifica se é para sair
		if (turnOffFlag) {
			break;
		}
		else if (resp == WAIT_TIMEOUT) {
			_tprintf(TEXT("\nAvião com id { %d } não respondeu por mais de %d segundos.\nA remove-lo!\n"), plane->id, TIMEOUT);
			removeAviaoDaLista(plane->id);
			break;
		}
		else {
			//obtem estado de avião
			int estadoAviao = 0;

			WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
			for(int i = 0; i < numAvioes; i++)
				if (plane->id == listaHandlesTimoutAvioes[i].idAviao) {
					estadoAviao = listaHandlesTimoutAvioes[i].flagAviaoDesligouse;
					break;
				}
			ReleaseMutex(hMutexAcederTimeoutAvioes);

			//0- não faz nada. 1 - remove avião da lista e desliga-se. 2- desliga-se
			if (estadoAviao == 1) {
				removeAviaoDaLista(plane->id);
				break;
			}
			else if (estadoAviao == 2) {			
				break;
			}
		}
		
	}

	CloseHandle(hAviao);	//fecha e remve da lista.

	return 0;
}

void removeAviaoDaLista(int id) {
	//remove o avião da lista
	WaitForSingleObject(hMutexAcederAvioes, INFINITE);
	WaitForSingleObject(hMutexAcederTimeoutAvioes, INFINITE);
	for (int i = 0; i < numAvioes; i++) {
		if (listaAvioes[i].id == id) {
			memcpy(&listaAvioes[i], &listaAvioes[numAvioes - 1], sizeof(listaAvioes[i]));
			break;
		}
	}
	
	//remove-se da lista

	for (int i = 0; i < numAvioes; i++) {
		if (listaHandlesTimoutAvioes[i].idAviao == id) {
			memcpy(&listaHandlesTimoutAvioes[i], &listaHandlesTimoutAvioes[numAvioes - 1], sizeof(listaHandlesTimoutAvioes[i]));
			--numAvioes;
			break;
		}
	}
	ReleaseMutex(hMutexAcederAvioes);
	ReleaseMutex(hMutexAcederTimeoutAvioes);
}
void escreveControl2Aviao(aviao* plane) {
	plane->debug++;
	WaitForSingleObject(hSemafotoEscritaControl2Aviao, INFINITE);		//obtem permissão apra escrever
	WaitForSingleObject(hMutexControl2Aviao, INFINITE);					//obtem permissão para mudar dados
	memcpy(&control2Aviao->planeBuffer[control2Aviao->proxEscrita], plane, sizeof(aviao));		//coloca o dados
	control2Aviao->proxEscrita == (control2Aviao->maxPos - 1) ? control2Aviao->proxEscrita = 0 : control2Aviao->proxEscrita++;		//aumenta o contador para escrita
	ReleaseMutex(hMutexControl2Aviao);								//liberta a auturização para modificar dados
	if (ReleaseSemaphore(hSemafotoLeituraControl2Aviao, 1, NULL) == 0)		//liberta autorização para ler o seeguinte
		_tprintf(TEXT("\nErro a libertar semáforo hSemafotoLeituraControl2Aviao -> %d\n"), GetLastError());
}


//passageiros

DWORD WINAPI criaConeccaoComPassageiros(LPVOID lpParam) {
	
	//cria lista de passageiros -> 200?
	listaPassageiros = malloc(TAM * sizeof(blocoPassageiro));
	numPassageiros = 0;
	


	//inicia mutex
	hMutexAcederPassageiros = CreateMutex(NULL, FALSE, MUTEX_ACEDER_LISTA_PASSAGEIROS);
	if (hMutexAcederPassageiros == NULL) {
		_tprintf(TEXT("Erro a hMutexAcederPassageiros ou hMutexAcederAvioes -> %d\n"), GetLastError());
		exit(1);
	}

	while (!turnOffFlag) {

		pBlocoPassageiro novoPassageiro = malloc(sizeof(blocoPassageiro));
		if (novoPassageiro == NULL)
		{
			_tprintf(TEXT("Malloc não consegiu alocar novo passageiro -> %d\n"), GetLastError());
			exit(1);
		}
		novoPassageiro->idAviao = -1;		//coloca id a -1, isto é um id que nunca acontece e é reconhecido como não tendo avião
		

		//PASSAGEIRO 2 CONTROL <- inbound
		novoPassageiro->hPipePassageiro2Control = CreateNamedPipe(PIPE_PASSAGEIRO_2_CONTROL, PIPE_ACCESS_INBOUND, PIPE_WAIT |					//cria o named pipe
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, PIPE_UNLIMITED_INSTANCES,
			sizeof(passageiro), sizeof(passageiro), 1000, NULL);
		if (novoPassageiro->hPipePassageiro2Control == INVALID_HANDLE_VALUE) {													//verifica se o pipe esta bom
			_tprintf(TEXT("Criar Named Pipe hPipePassageiro2Control -> %d\n"), GetLastError());			
			exit(-1);
		}

		

		//CONTROL 2 PASSAGEIRO -> outbound
		novoPassageiro->hPipeControl2Passageiro = CreateNamedPipe(PIPE_CONTROL_2_PASSAGEIRO, PIPE_ACCESS_OUTBOUND, PIPE_WAIT |					//cria o named pipe
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, PIPE_UNLIMITED_INSTANCES,
			sizeof(passageiro), sizeof(passageiro), 1000, NULL);
		if (novoPassageiro->hPipeControl2Passageiro == INVALID_HANDLE_VALUE) {													//verifica se o pipe esta bom
			_tprintf(TEXT("Criar Named Pipe hPipeControl2Passageiro -> %d\n"), GetLastError());
			exit(-1);
		}
		

		//espera que se conectem aos dois pipes
		if (!ConnectNamedPipe(novoPassageiro->hPipePassageiro2Control, NULL) && GetLastError() != 535) {												//espera que um cliente se conecte
			_tprintf(TEXT("Erro na espera pela ligação do passageiro ao pipe hPipePassageiro2Control -> %d\n"), GetLastError());
			exit(-1);
		}
		int a;
		if ( !(ConnectNamedPipe(novoPassageiro->hPipeControl2Passageiro, NULL)) && GetLastError() != 535) {												//espera que um cliente se conecte
			_tprintf(TEXT("Erro na espera pela ligação do passageiro ao pipe hPipeControl2Passageiro -> %d\n"), GetLastError());
			exit(-1);
		}


		HANDLE threadWaitConecaoPassageiros = CreateThread(NULL, 0, conecaoComPassageiro, novoPassageiro, 0, NULL);
		if (threadWaitConecaoPassageiros == NULL) {
			_tprintf(TEXT("Erro a criar thread que fala com um passageiro! -> %d\n"), GetLastError());
			exit(1);
		}

	}
	return 0;
}


/*
	//estados passageiro										quem envia		/		quem recebe
	#define ESTADO_CONECTAR_PASSAGEIRO 1						passageiro				controlo
	#define ESTADO_ARGUMENTOS_INVALIDOS 2						controlo				passageiro
	#define ESTADO_ESPERA_POR_AVIAO 3							controlo				passageiro
	#define ESTADO_EMBARCOU_PASSAGEIRO 4						controlo				passageiro
	#define ESTADO_PASSAGEIRO_VOO 5								controlo				passageiro
	#define ESTADO_PASSAGEIRO_TIMEOUT 6							passageiro				controlo
	#define ESTADO_PASSAGEIRO_CHEGOU_DESTINO 7					passageiro e controlo	passageiro e controlo
	#define ESTADO_PASSAGEIRO_QUER_DESLIGAR 8					passageiro				controlo
	#define ESTADO_CONTROLO_DESLIGA_PASSAGEIROS 9				controlo				passageiro e controlo

	Controlador:
	O controlador cria os pipes e fica à espera que alguem se conecte aos pipes
	Quando alguem se conecta cria uma thread que trata de ler as coisas.
	No inicio, essa thread espera uma mensagem com o ESTADO_CONECTAR_PASSAGEIROS, se as coisas estiverem mal deevolve ESTADO_ARGUMENTOS_INVALIDOS.
	Se estiver tudo bem, adiciona à lista o avião com ambas as HANDLES para os pipes e responde com ESTADO_ESPERA_POR_AVIAO. Este é o único momento em que esta thread escreve o que quer que seja.
	Quem trata de escrever é a thread geral que também fala com os aviões!
	Esta thread limita-se a ler as mensagens: ESTADO_PASSAGEIRO_TIMEOUT e ESTADO_PASSAGEIRO_QUER_DESLIGAR, reagindo às mesmas.
	Quando um aviao chega ao destino com os passageiros, a thread geral de comnunicação envia para o programa passageiro ESTADO_PASSAGEIRO_CHEGOU_DESTINO. O programa passageiro, ao receber esta mensagem devolve uma mensagem com o mesmo estado!

	Passageiro:
	O passageiro tem duas threads distintas da main.
	Uma das threads fica à espera de um input do user para desligar o programa. Quando o faz, envia para o controlador o estado ESTADO_PASSAGEIRO_QUER_DESLIGAR.
	A outra thread é de leitura e fica a ler as mensagens do controlador. As mensagens podem ser de qualquer tipo acima ligado e deverá reagir de acordo.
	Reações da thread de leitura:
				ESTADO_ARGUMENTOS_INVALIDOS -> indica que existem erros no nome das cidades colocadas e sai
				ESTADO_ESPERA_POR_AVIAO -> indica que o passageiro encontra-se à espera de uma avião
				ESTADO_EMBARCOU_PASSAGEIRO -> indica que o passageiro foi embarcado num avião
				ESTADO_PASSAGEIRO_VOO -> indica que passageiro está em voo. Apresenta as suas coordenadas
				Quando existe um timeout de espera pelo avião envia: ESTADO_PASSAGEIRO_TIMEOUT
				ESTADO_PASSAGEIRO_CHEGOU_DESTINO -> indica que chegou ao destino. Devolve o passageiro recebido para o controlo poder desligar a thread. retorna à main
				ESTADO_CONTROLO_DESLIGA_PASSAGEIROS -> indica que o controlo desligou tudo. Desvolve o passageiro recebido para o controlo poder desligar a thread
	A main do programa passageiro encontra-se a fazer WaitForMultipleObject, sendo que assim que desbloqueia uma mata a outra e desliga-se


	o passageiro conetca-se com o estado ESTADO_CONECTAR_PASSAGEIRO, o seu nome, o aeroporto inicial e o aeroporto de destino
	o controlador recebe o estado ESTADO_CONECTAR_PASSAGEIRO, verifica o se os aeroportos existem.
	Se não existirem, devolve ESTADO_ARGUMENTOS_INVALIDOS e desliga a thread onde troca mensagens.
		O passageiro lê ESTADO_ARGUMENTOS_INVALIDOS, mostra no ecra o erro, e acaba a thread de leitura.
	Se tudo estiver bem, o controlador coloca as coordenadas no passageiro, coloca-o na lista de passageiros e devolve ESTADO_ESPERA_POR_AVIAO.
	Quando o passageiro recebe ESTADO_ESPERA_POR_AVIAO, um timeout é activado.
	Se o timeout chegar ao fim -> ainda a ver como fazer <- então o passageiro escreve ESTADO_PASSAGEIRO_TIMEOUT e procede a sair da thread de onde enviou essa mensagem, desligando o programa.
	Se não existiu timeout, entao o passageiro foi colocado num avião. Quando o passageiro é colocado num avião é enviado ESTADO_EMBARCOU_PASSAGEIRO, o programa passageiro deverá indicar isso
		Note-se que, a partir daqui, o timeout poderá ser desligado (ou mantido, idk....)
	A posição deverá ser atualizada a cada segundo, recebendo ESTADO_PASSAGEIRO_VOO e mostrando no ecra as coordenadas.
	Quando o controlador percebe que o avião chega ao destino, desembarca todas as pessoas, pelo que envia aos passageiros o estado ESTADO_PASSAGEIRO_CHEGOU_DESTINO.
	Quando o passageiro recebe o estado ESTADO_PASSAGEIRO_CHEGOU_DESTINO, compreende que chegou ao fim. Desliga a thread de leitura e, poesteiorimente, o programa.
	A qualquer momento o uitlizador do passageiro pode querer sair. Para isso escreve "sair". A thread com o utilizador percebe isso e envia o estado ESTADO_PASSAGEIRO_QUER_DESLIGAR, dendo que, depois desliga-se
		O fecho dessa thread acaba por deligar o programa -> WaitForMultipleObjects().
	Quando o controlador recebe ESTADO_PASSAGEIRO_QUER_DESLIGAR remove o passageiro da lista de passageiros e desliga a thread relacionada com o mesmo (desligando o pipe idk)
*/
DWORD WINAPI conecaoComPassageiro(LPVOID lpParam) {
	blocoPassageiro blocoPassag = *(pBlocoPassageiro)lpParam;
	passageiro passag;
	DWORD n;

	//lê o estado de coneção incial com os dados
	BOOL ret = ReadFile(blocoPassag.hPipePassageiro2Control, &passag, sizeof(passag), &n, NULL);								//lê o pipe usando "ReadFile()"
	if (!ret || n != sizeof(passageiro)) {
		_tprintf(TEXT("Erro a ler do readFile -> %d\nA desligar coneção com passageiro %s\n"), GetLastError(), passag.nome);
		//exit(1);
		//fecha os pipes
		CloseHandle(blocoPassag.hPipeControl2Passageiro);
		CloseHandle(blocoPassag.hPipePassageiro2Control);

		//return
		return 0;
	}

	_tprintf(TEXT("O passageiro <%s> ligou-se à rede, a verificar credenciais!\n"), passag.nome);

	//verifica se o nome dos aeroportos é valido
	//se não for devolve ESTADO_ARGUMENTOS_INVALIDOS e desliga;
	//se for adiciona o passageiro à lista de passageiros e devolve com estado ESTADO_ESPERA_POR_AVIAO

	//verifica aeroporto de destino
	boolean flagAerportoDestinoExiste = FALSE;
	boolean flagAerportoPartidaExiste = FALSE;
	WaitForSingleObject(hMutexAcederAeroportos, INFINITE);		//pede mutex para aceder aos aeroportos
	for (int i = 0; i < numAeroportos; i++) {
		if (wcscmp(listaAeroportos[i].nome, passag.aeroportoDestino) == 0) {
			flagAerportoDestinoExiste = TRUE;
			break;
		}
	}
	if (flagAerportoDestinoExiste) {
		for (int i = 0; i < numAeroportos; i++) {
			if (wcscmp(listaAeroportos[i].nome, passag.aeroportoInicial) == 0) {
				flagAerportoPartidaExiste = TRUE;
				passag.x = listaAeroportos[i].x;
				passag.y = listaAeroportos[i].y;
				break;
			}
		}
	}
	ReleaseMutex(hMutexAcederAeroportos);						//liberta o mutex para aceder aos aeroportos

	//se estiver mal envia ESTADO_ARGUMENTOS_INVALIDOS, fecha pipes e return
	if (!flagAerportoDestinoExiste || !flagAerportoPartidaExiste) {
		_tprintf(TEXT("O passageiro %s colocou argumentos inválidos!\n"), passag.nome);
		//envia mensagem sobre com argumentos errados
		passag.estado = ESTADO_ARGUMENTOS_INVALIDOS;
		if (!WriteFile(blocoPassag.hPipeControl2Passageiro, &passag, sizeof(passag), &n, NULL)) {			//escreve a frase no pipe
			_tprintf(TEXT("Erro a enviar mensagem de argumentos inválidos para o pipe -> %d\n"), GetLastError());
			//exit(-1);
		}
		//fecha os pipes
		CloseHandle(blocoPassag.hPipeControl2Passageiro);
		CloseHandle(blocoPassag.hPipePassageiro2Control);

		//return
		return 0;
	}

	
	//coloca o estado e os dados do passageiro
	passag.estado = ESTADO_ESPERA_POR_AVIAO;
	memcpy(&blocoPassag.passg, &passag, sizeof(passag));
	
	//adiciona à lista de passageiros
	WaitForSingleObject(hMutexAcederPassageiros, INFINITE);

	memcpy(&listaPassageiros[numPassageiros++], &blocoPassag, sizeof(blocoPassag));

	ReleaseMutex(hMutexAcederPassageiros);

	_tprintf(TEXT("O passageiro %s encontra-se à espera de avião para embarcar!\n"), passag.nome);
	
	//entra em ciclo até receber uma das 3/4 respostas possiveis
	while (!turnOffFlag) {
		ret = ReadFile(blocoPassag.hPipePassageiro2Control, &passag, sizeof(passag), &n, NULL);								//lê o pipe usando "ReadFile()"
		if (!ret || n != sizeof(passageiro)) {
			_tprintf(TEXT("\nErro a ler do readFile -> %d\nA desligar coneção com cliente <%s>\n"), GetLastError(), passag.nome);
			//fecha os pipes
			CloseHandle(blocoPassag.hPipeControl2Passageiro);
			CloseHandle(blocoPassag.hPipePassageiro2Control);

			//return
			return 0;
		}

		if (passag.estado == ESTADO_PASSAGEIRO_TIMEOUT) {
			_tprintf(TEXT("Passageiro %s fartou-se de esperar e foi embora\n"), passag.nome);
			break;
		}
		else if (passag.estado == ESTADO_PASSAGEIRO_QUER_DESLIGAR) {
			_tprintf(TEXT("Passageiro %s decidiu desligar\n"), passag.nome);
			break;
		}
		else if (passag.estado == ESTADO_PASSAGEIRO_CHEGOU_DESTINO) {
			_tprintf(TEXT("Passageiro %s chegou ao seu destino!\n"), passag.nome);
			break;
		}
		else if (passag.estado == ESTADO_CONTROLO_DESLIGA_PASSAGEIROS) {
			//não precisa de remover da lista
			CloseHandle(blocoPassag.hPipeControl2Passageiro);
			CloseHandle(blocoPassag.hPipePassageiro2Control);
			return 0;
		}
		
	}
	//remove da lista de passageiros
	WaitForSingleObject(hMutexAcederPassageiros, INFINITE);

	//procura pelo passageiro na lista
	for(int i = 0; i < numPassageiros; i++)
		//quando encpontra
		if ((wcscmp(listaPassageiros[i].passg.nome, passag.nome))==0) {
			//troca o meu passageiro pelo "ultimo" e reduz o contador
			memcpy(&listaPassageiros[i], &listaPassageiros[numPassageiros--], sizeof(blocoPassag));	
			break;
		}
	ReleaseMutex(hMutexAcederPassageiros);
	//fecha pipes
	CloseHandle(blocoPassag.hPipeControl2Passageiro);
	CloseHandle(blocoPassag.hPipePassageiro2Control);

	return 0;
}

void enviaMensagemPipe(blocoPassageiro blocoPass) {
	DWORD n;
	if (!WriteFile(blocoPass.hPipeControl2Passageiro, &blocoPass.passg, sizeof(passageiro), &n, NULL)) {			//escreve a frase no pipe
		_tprintf(TEXT("Erro a enviar mensagem de argumentos inválidos para o pipe -> %d\n"), GetLastError());
		exit(-1);
	}
}


void atualizaAviaoNaLista(aviao data) {
	//atualiza avião na lista de aviões
	WaitForSingleObject(hMutexAcederAvioes, INFINITE);
	for (int i = 0; i < numAvioes; i++) {
		if (listaAvioes[i].id == data.id)
			memcpy(&listaAvioes[i], &data, sizeof(data));
	}
	ReleaseMutex(hMutexAcederAvioes);
}

