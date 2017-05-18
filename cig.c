#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define DEBUG		1

#if DEBUG > 0
#define DPRT(S, ...) fprintf(stderr, S, ##__VA_ARGS__)
#else
#define DPRT(S, ...)
#endif

#define SAMP_CNT_MAX		128

#define CARRY_SAMPLE		3
#define CARRY_MOLECULE		10

#define PROJECT_CNT		3

#define POS_CNT			5

#define M_TYPE_CNT		5
#define IND_A			0
#define IND_B			((IND_A)+1)
#define IND_C			((IND_B)+1)
#define IND_D			((IND_C)+1)
#define IND_E			((IND_D)+1)

/* get the index for type('A', 'B', 'C', 'D', 'E') */
#define TYPE_IND(type)		(type-'A'+IND_A)

enum fsm_en{
	FSM_INIT = 0,
	FSM_SAMPLES,
	FSM_DIAGNOSIS,
	FSM_MOLECULES,
	FSM_LABORATORY,
	FSM_WAIT
};

struct fsm_st{
	enum fsm_en	prev;
	enum fsm_en	cur;
	enum fsm_en	next;
};

int distance[5][5] = {	{0, 2, 2, 2, 2},
			{2, 0, 3, 3, 3},
			{2, 3, 0, 3, 4},
			{2, 3, 3, 0, 3},
			{2, 3, 4, 3, 0}};

struct sample_data{
	int id;
	int carry;
	int rank;
	int health;
	int cost;
	int Ms[M_TYPE_CNT];
	//int A; int B; int C; int D; int E;
} samples[SAMP_CNT_MAX];

struct proj_st{
	int Ms[M_TYPE_CNT];
	//int A; int B; int C; int D; int E;
} projects[PROJECT_CNT];

enum samp_state_en{
	SAMP_STATE_INIT=0,
	SAMP_STATE_ANAYLZE,
	SAMP_STATE_ING,
	SAMP_STATE_READY
};

/* have not analyze */
#define S_STATE_INIT		0
/* analyze ok */
#define S_STATE_READY	1
/* gather molecules for it */
#define S_STATE_ING		2
/* gather molecules ok */
#define S_STATE_OK		3

struct carry_sample_list{
	int id;
	int state;
};

struct bot_st{
	char target[16];
	int eta;
	int score;
	int hMs[M_TYPE_CNT];	/* the molecules hold */
	int eMs[M_TYPE_CNT];	/* the molecules expertise*/
	//int samples[CARRY_SAMPLE];
	struct carry_sample_list samp_list[CARRY_SAMPLE];
	int samp_cnt;
	int molecules;
	int e_cnt;
};

struct bot_st Go;
struct bot_st other;

int get_carry_S(void)
{
	return Go.samp_cnt;
}

int get_carry_M(void)
{
	return Go.samp_cnt;
}

void bot_init_sample(void)
{
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		Go.samp_list[i].id = -1;
		Go.samp_list[i].status = S_STATE_INIT;
	}
}

int bot_carry_sample(int id)
{
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		if(id == Go.samp_list[i].id)
			return 0;
	}

	for(i = 0; i < CARRY_SAMPLE; i++){
		if(-1 == Go.samp_list[i].id){
			Go.samp_list[i].id = id;
			Go.samp_cnt += 1;
			return 0;
		}
	}

	DPRT("[PANIC][%s]WTF?\n", __FUNCTION__);
	return -1;
}

/*
 * @type	'A','B','C','D','E'
 */
int bot_carry_molecule(int type, int id)
{
	DPRT("[D]carry the M %c\n", type);
	printf("CONNECT %c give me %c for S[%d]\n", type, type, id);
	update_status();

	return 0;
}

int bot_del_samp(int id)
{
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++)
		if(id == Go.samp_list[i].id){
			Go.samp_list[i].id = -1;
			Go.samp_list[i].status = S_STATE_INIT;
			Go.samp_cnt -= 1;
			return 0;
		}

	DPRT("[PANIC][%s]WTF?\n", __FUNCTION__);
	return -1;
}

int bot_get_exp_cnt()
{
	return Go.e_cnt;
}

int bot_get_Mcnt(int type)
{
	int ind = TYPE_IND(type);
	if(type){
		return Go.hMs[ind]+Go.eMs[ind];
	}

	int cnt = 0;
	int i;

	for(i = 0; i < M_TYPE_CNT; i++){
		cnt += Go.hMs[ind] + Go.eMs[ind];
	}

	return cnt;
}

int aMs[M_TYPE_CNT];
//int aA;
//int aB;
//int aC;
//int aD;
//int aE;

/* Collect sample data files from the SAMPLES module */
/*
 * return	0	collect ok
 * 		-1	can not collect any more
 *
 */
int collect_sample(void)
{
	if(Go.samp_cnt >= 3)
		return -1;

	if(bot_get_exp_cnt() > 5 && get_carry_S() >= 2)
		printf("CONNECT 3\n");
	else
		printf("CONNECT 2\n");

	return 0;
}

/* Analyze sample data files at the DIAGNOSIS module to get a list of molecules for the associated medicine. */
/*
 * return	0	analyze ok
 *		-1	have nothing to analyze
 */
int analyze_sample(void)
{
	DPRT("[D]start analyze samples.\n");
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		if(Go.samp_list[i].id >= 0 && samples[Go.samp_list[i].id].health < 0){
			DPRT("[D]want analyze %d\n", Go.samp_list[i].id);
			printf("CONNECT %d analyze %d\n", Go.samp_list[i].id, Go.samp_list[i].id);
			update_status();
		}
	}

	return 0;
}

int cmp_available(const void *a, const void *b)
{
	return *((int *)a + 1) - *((int *)b + 1);
}

/* gather molecules for one sample */
/*
 * return	0	gather ok
 * 		-1	failed, maybe lack some molecules
 */
int gather_molecules_by_samp_id(int id)
{
	struct sample_data *s = samples+id;
	struct carry_sample_list *node = NULL;
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		if(id == Go.samp_list[i].id){
			node = Go.samp_list + i;
		}
	}

	if(!node){
		DPRT("[PANIC][%s]WTF?\n", __FUNCTION__);
		return -1;
	}
	if(SAMP_GATHER_OK == node->status)
		return 1;

	node->status = SAMP_GATHER_ING;

	while(1){
		struct tmp_available{
			int type;
			int cnt;
		} AT[5] = {{'A', aMs[0]},{'B', aMs[1]},{'C', aMs[2]},{'D', aMs[3]},{'E', aMs[4]}};
		/* sort available molecules */
//struct sample_data{ int id; int carry; int rank; int health; int cost; int Ms[];} samples[SAMP_CNT_MAX];
		qsort(AT, 5, sizeof(struct tmp_available), &cmp_available);
for(i = 0; i < 5; i++){
	DPRT("[D]M %d %c %d\n", i, AT[i].type, AT[i].cnt) ;
}
		for(i = 0; i < 5; i++){
			int ind = TYPE_IND(AT[i].type);
			DPRT("[CC]this need %c ? %s\n", AT[i].type, s->Ms[ind] - Go.hMs[ind]-Go.eMs[ind] > 0?"YES":"NO");
			if(s->Ms[ind] - Go.hMs[ind]-Go.eMs[ind] > 0){
				/* need this type at least 1 */
				fprintf(stderr, "[D]S id[%d] n33d %c\n", id, AT[i].type);
				if(AT[i].cnt > 0){
					bot_carry_molecule(AT[i].type, id);
					break;
				}else{
					DPRT("[PANIC]OH-NO!!What can i do next??? i need %c\n", AT[i].type);
					node->status = S_STATE_INIT;
show_info();
					return 2;
				}
			}

//DPRT("[D]S id%d need %c ? %s s->B=%d avtypes[i].cnt=%d\n", id, avtypes[i].type, this?"Yes":"No", s->B, avtypes[i].cnt);
			if(Go.molecules >= CARRY_MOLECULE)
				return -1;
		}
		if(i == 5){
			DPRT("[D]gather molecules for sample[%d] ok\n", id);
			node->status = SAMP_GATHER_OK;
			return 0;
		}
	}

	return -1;
}

/*
 * set the best match sample to S_STATE_ING, according to the carried molecules
 */
int get_best_match_S()
{
	int i;
	struct carry_sample_list *list = Go.samp_list;
	int *hMs = Go.hMs;
	int *eMs = Go.eMs;

	for(i = 0; i < CARRY_SAMPLE; i++){
		DPRT("[TTT][%s][%d][%d]\n", __FUNCTION__, (list+i)->id, list[i].status);
		if(-1 != (list+i)->id && SAMP_GATHER_ING == list[i].status)
			return Go.samp_list[i].id;
	}

	for(i = 0; i < CARRY_SAMPLE; i++){
		int id = list[i].id;
		DPRT("[D]look at [%d]\n", id);
		if(id >= 0 && _is_enough(id) && SAMP_GATHER_INIT == list[i].status)
			return id;
	}
	for(i = 0; i < CARRY_SAMPLE; i++){
		if(-1 != (list+i)->id && SAMP_GATHER_OK == list[i].status)
			return Go.samp_list[i].id;
		DPRT("[D]for SAMP_GATHER_OK\n");
	}

	return 0;
}

/* test sample(id) can be produce if get all molecules */
int _is_enough(int id)
{
	return aMs[IND_A]+bot_get_Mcnt('A') >= samples[id].Ms[IND_A]		\
		&& aMs[IND_B]+bot_get_Mcnt('B') >= samples[id].Ms[IND_B]	\
		&& aMs[IND_C]+bot_get_Mcnt('C') >= samples[id].Ms[IND_C]	\
		&& aMs[IND_D]+bot_get_Mcnt('D') >= samples[id].Ms[IND_D]	\
		&& aMs[IND_E]+bot_get_Mcnt('E') >= samples[id].Ms[IND_E];
}

int get_next_samp()
{
	//int is_enough[CARRY_SAMPLE];

	return -1;
}

/* Gather required molecules for the medicines at the MOLECULES module */
int gather_molecules(void)
{
	int id;

	if(get_carry_M() < CARRY_MOLECULE){
		if((id = get_next_samp()) < 0){
			if(get_carry_S() >= CARRY_SAMPLE){
				DPRT("[OO] I can not due this, wait for write\n");
				exit(1);
			}
			DPRT("[D]no samp can gather, n33d to collect new samples.\n");
			return -1;
		}
		if(id = gather_molecules_by_samp_id(id) == 2)
			return 1;
	}

	return 0;
}

int produce_medichine(void)
{
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		DPRT("[T]carry %d[%d]\n", i, Go.samp_list[i].id);
		struct sample_data *s = samples + Go.samp_list[i].id;
		if(Go.samp_list[i].id != -1 && bot_get_Mcnt('A') >= s->Ms[IND_A]			\
			&& bot_get_Mcnt('B') >= s->Ms[IND_B] && bot_get_Mcnt('C') >= s->Ms[IND_C]	\
			&& bot_get_Mcnt('D') >= s->Ms[IND_D] && bot_get_Mcnt('E') >= s->Ms[IND_E]){

			DPRT("[D]want produce ID[%d %d]\n", Go.samp_list[i].id, samples[Go.samp_list[i].id].id);
//DPRT("[D-Show][bot] A[%d]B[%d]C[%d]D[%d]E[%d] ids[%d][%d][%d]\n", Go.hA, Go.hB, Go.hC, Go.hD, Go.hE, Go.samples[0], Go.samples[1], Go.samples[2]);
			printf("CONNECT %d produce %d\n", Go.samp_list[i].id, Go.samp_list[i].id);
			bot_del_samp(Go.samp_list[i].id);
			update_status();
		}
	}

	return 0;
}

int update_status(void)
{
	int i;

	for(i = 0; i < 2; i++){
		struct bot_st *this;
		this = 0 == i ? &Go: &other;
		int A, B, C, D, E;
		int eA, eB, eC, eD, eE;
		scanf("%s%d%d%d%d%d%d%d%d%d%d%d%d", this->target, &this->eta, &this->score, &A, &B, &C, &D, &E, &eA, &eB, &eC, &eD, &eE);
		this->hMs[IND_A] = A;
		this->hMs[IND_B] = B;
		this->hMs[IND_C] = C;
		this->hMs[IND_D] = D;
		this->hMs[IND_E] = E;
		this->eMs[IND_A] = eA;
		this->eMs[IND_B] = eB;
		this->eMs[IND_C] = eC;
		this->eMs[IND_D] = eD;
		this->eMs[IND_E] = eE;
		this->molecules = A + B + C + D + E;
		this->e_cnt = eA + eB + eC + eD + eE;
		DPRT("[D][%s]BOT at[%s],eta[%d]score[%d],A[%d]B[%d]C[%d]D[%d]E[%d] expertisxpertiseeA[%d]B[%d]C[%d]D[%d]E[%d]\n", __FUNCTION__, this->target, this->eta, this->score, this->hMs[IND_A], this->hMs[IND_B], this->hMs[IND_C], this->hMs[IND_D], this->hMs[IND_E], this->eMs[IND_A], this->eMs[IND_B], this->eMs[IND_C], this->eMs[IND_D], this->eMs[IND_E]);
	}
	//int aA, aB, aC, aD, aE;
	scanf("%d%d%d%d%d", aMs+IND_A, aMs+IND_B, aMs+IND_C, aMs+IND_D, aMs+IND_E);
	DPRT("[D][%s]available A[%d] B[%d] C[%d] D[%d] E[%d]\n", __FUNCTION__, aMs[IND_A], aMs[IND_B], aMs[IND_C], aMs[IND_D], aMs[IND_E]);
	int sampleCount;
	scanf("%d", &sampleCount);
	DPRT("[Debug]there are [%d] sample datas.\n", sampleCount);
	for (int i = 0; i < sampleCount; i++) {
		int id;
		int carriedBy;
		int rank;
		char expertiseGain[2];
		int health;
		int costA;
		int costB;
		int costC;
		int costD;
		int costE;
		scanf("%d%d%d%s%d%d%d%d%d%d", &id, &carriedBy, &rank, expertiseGain, &health, &costA, &costB, &costC, &costD, &costE);
		struct sample_data *data = samples+id;
		DPRT("[D][%s]id[%d]c[%d]h[%d]cA[%d]cB[%d]cC[%d]cD[%d]cE[%d]gain[%c]\n", __FUNCTION__, id, carriedBy, health, costA, costB, costC, costD, costE, expertiseGain[0]);
		data->id = id;
		data->health = health;
		data->rank = rank;
		data->cost = costA + costB + costC + costD + costE;
		data->carry = carriedBy;
		data->Ms[IND_A] = costA;
		data->Ms[IND_B] = costB;
		data->Ms[IND_C] = costC;
		data->Ms[IND_D] = costD;
		data->Ms[IND_E] = costE;
		/* carryed by me */
		if(0 == carriedBy){
			//DPRT("[D] ID[%d] is carry by Go.\n", id);
			bot_carry_sample(id);
		}
	}
	DPRT("[Debug][%s]now Go carry [%d] samples\n", __FUNCTION__, Go.samp_cnt);

	return 0;
}

int show_info()
{
	int i;

	for(i = 0; i < SAMP_CNT_MAX; i++){
		if(samples[i].id < 0)
			continue;
		DPRT("[D-Show]ID[%d]H[%d]A[%d]B[%d]C[%d]D[%d]E[%d]\n", samples[i].id, samples[i].health, samples[i].Ms[IND_A], samples[i].Ms[IND_B], samples[i].Ms[IND_C], samples[i].Ms[IND_D], samples[i].Ms[IND_E]);
	}
	//DPRT("[D-Show][bot] A[%d]B[%d]C[%d]D[%d]E[%d] ids[%d][%d][%d]\n", Go.hA, Go.hB, Go.hC, Go.hD, Go.hE, Go.samples[0], Go.samples[1], Go.samples[2]);

	return 0;
}

int produce_continue()
{
	int i;

	for(i = 0; i < CARRY_SAMPLE; i++){
		if(-1 != Go.samp_list[i].id)
			return 1;
	}

	return 0;
}

int game_init(void)
{
	int i;

	for(i = 0; i < SAMP_CNT_MAX; i++){
		samples[i].id = -1;
	}
	bot_init_sample();

	return 0;
}

int main(void)
{
	int projectCount;
	scanf("%d", &projectCount);
	for (int i = 0; i < projectCount; i++) {
		int a, b, c, d, e;
		scanf("%d%d%d%d%d", &a, &b, &c, &d, &e);
		DPRT("project[%d] A[%d]B[%d]C[%d]D[%d]E[%d]\n", i, a, b, c, d, e);
	}

	int first = 1;
	struct fsm_st fsm = {FSM_INIT, FSM_INIT, FSM_INIT};
	// game loop
	while(1){
		//fsm
		switch(fsm.next){
		case FSM_INIT:
			if(first){
				game_init();
				clean_input();
				first = 0;
				printf("GOTO SAMPLES get samples\n");
				update_status();
				break;
			}
			if(Go.eta){
				printf("WAIT ETA %d\n", Go.eta);
fprintf(stderr, "stderr this\n");
				update_status();
			}else{
				fsm.prev = fsm.next;
				fsm.next = FSM_SAMPLES;
			}
			break;
		case FSM_SAMPLES:
			DPRT("[Debug]FSM into SAMPLES\n");
			if(Go.eta){
				printf("WAIT ETA %d\n", Go.eta);
				update_status();
				break;
			}
			if(!collect_sample()){
				update_status();
				break;
			}
			printf("GOTO DIAGNOSIS\n");
			update_status();
			fsm.prev = fsm.next;
			fsm.next = FSM_DIAGNOSIS;
			break;
		case FSM_DIAGNOSIS:
			DPRT("[Debug]FSM into DIAGNOSIS\n");
			if(Go.eta){
				printf("WAIT ETA %d\n", Go.eta);
				Go.eta -= 1;
				update_status();
				break;
			}
			analyze_sample();
			printf("GOTO MOLECULES\n");
			update_status();
			fsm.prev = fsm.next;
			fsm.next = FSM_MOLECULES;
			break;
		case FSM_MOLECULES:
			DPRT("[Debug]FSM into MOLECULES\n");
			if(Go.eta){
				printf("Dest MOLECULES ETA[%d]\n", Go.eta);
				Go.eta -= 1;
				update_status();
				break;
			}
			if(gather_molecules()){
				printf("GOTO SAMPLES goto get new\n");
				update_status();
				fsm.prev = fsm.next;
				fsm.next = FSM_SAMPLES;
				break;
			}
			printf("GOTO LABORATORY\n");
			fsm.prev = fsm.next;
			fsm.next = FSM_LABORATORY;
			update_status();
			break;
		case FSM_LABORATORY:
			DPRT("[Debug]FSM into LABORATORY\n");
			if(Go.eta){
				printf("Dest LABORATORY ETA[%d]\n", Go.eta);
				Go.eta -= 1;
				update_status();
				break;
			}
			produce_medichine();
			fsm.prev = fsm.next;
			if(produce_continue()){
				printf("GOTO MOLECULES\n");
				fsm.next = FSM_MOLECULES;
				update_status();
			}else{
				printf("GOTO SAMPLES\n");
				fsm.next = FSM_SAMPLES;
				update_status();
			}
			break;
		case FSM_WAIT:
			//fsm_state = FSM_DIAGNOSIS;
			//break;
		default:
			DPRT("[Debug] unknown state in FSM.\n");
		}
	}

	return 0;
}

int clean_input(void)
{
	DPRT("[CLEAN]\n");
	for (int i = 0; i < 2; i++) {
		scanf("%*s%*d%*d%*d%*d%*d%*d%*d%*d%*d%*d%*d%*d");
	}
	scanf("%*d%*d%*d%*d%*d");
	int sampleCount;
	scanf("%d", &sampleCount);
	for (int i = 0; i < sampleCount; i++) {
		scanf("%*d%*d%*d%*s%*d%*d%*d%*d%*d%*d");
	}

	return 0;
}
