#include "core.h"
#include "common.h"
#include <dplasmatypes.h>
#include <data_dist/matrix/two_dim_rectangle_cyclic.h>
#include <interfaces/superscalar/insert_function.h>


#define MAX_ARGS  4

enum regions {
  TILE_FULL,
};

static int
test_task1(parsec_execution_stream_t *es, parsec_task_t *this_task)
{
    (void)es;
    int i, j;
    double *data1;

    parsec_dtd_unpack_args(this_task, &i, &j, &data1);
    
    *data1 = 0.0;
    printf("i %d, j %d, data1 %f\n", i, j, *data1);

    return PARSEC_HOOK_RETURN_DONE;
}

static int
test_task2(parsec_execution_stream_t *es, parsec_task_t *this_task)
{
    (void)es;
    int i, j;
    double *data1, *data2;

    parsec_dtd_unpack_args(this_task, &i, &j, &data1, &data2);
    
    *data2 = *data1 + 1.0;
    printf("i %d, j %d, data2 %f\n", i, j, *data2);

    return PARSEC_HOOK_RETURN_DONE;
}

static int
test_task3(parsec_execution_stream_t *es, parsec_task_t *this_task)
{
    (void)es;
    int i, j;
    double *data1, *data2, *data3;

    parsec_dtd_unpack_args(this_task, &i, &j, &data1, &data2, &data3);
    
    *data3 = *data1 + *data2 + 1.0;
    printf("i %d, j %d, data3 %f\n", i, j, *data3);

    return PARSEC_HOOK_RETURN_DONE;
}

static int
test_task4(parsec_execution_stream_t *es, parsec_task_t *this_task)
{
    (void)es;
    int i, j;
    double *data1, *data2, *data3, *data4;

    parsec_dtd_unpack_args(this_task, &i, &j, &data1, &data2, &data3, &data4);
    
    *data4 = *data1 + *data2 + *data3 + 1.0;
    printf("i %d, j %d, data4 %f\n", i, j, *data4);

    return PARSEC_HOOK_RETURN_DONE;
}

struct ParsecApp : public App {
  ParsecApp(int argc, char **argv);
  ~ParsecApp();
  void execute_main_loop();
  void execute_timestep(long t);
private:
  void insert_task(int num_args, int i, int j, std::vector<parsec_dtd_tile_t*> &args);
private:
  parsec_context_t* parsec;
  parsec_taskpool_t *dtd_tp;
  two_dim_block_cyclic_t *__dcC;
  two_dim_block_cyclic_t dcC;
  int rank;
  int nodes;
  int cores;
  int gpus;
  int P;
  int Q;
  int M;
  int N;
  int K;
  int NRHS;
  int LDA;
  int LDB;
  int LDC;
  int IB;
  int MB;
  int NB;
  int SMB;
  int SNB;
  int HMB;
  int HNB;
  int MT;
  int NT;
  int KT;
  int check;
  int check_inv;
  int loud;
  int scheduler;
  int random_seed;
  int matrix_init;
  int butterfly_level;
  int async;
  int iparam[IPARAM_SIZEOF];
};

void ParsecApp::insert_task(int num_args, int i, int j, std::vector<parsec_dtd_tile_t*> &args)
{
  switch(num_args) {
  case 1:
    parsec_dtd_taskpool_insert_task(dtd_tp, test_task1,    0,  "test_task1",
                                    sizeof(int),    &i,  VALUE,
                                    sizeof(int),    &j,  VALUE,
                                    PASSED_BY_REF,  args[0], INOUT | TILE_FULL | AFFINITY,
                                    PARSEC_DTD_ARG_END);
    break;
  case 2:
    parsec_dtd_taskpool_insert_task(dtd_tp, test_task2,    0,  "test_task2",
                                    sizeof(int),    &i,  VALUE,
                                    sizeof(int),    &j,  VALUE,
                                    PASSED_BY_REF,  args[1], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[0], INOUT | TILE_FULL | AFFINITY,
                                    PARSEC_DTD_ARG_END);
    break;
  case 3:
    parsec_dtd_taskpool_insert_task(dtd_tp, test_task3,    0,  "test_task3",
                                    sizeof(int),    &i,  VALUE,
                                    sizeof(int),    &j,  VALUE,
                                    PASSED_BY_REF,  args[1], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[2], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[0], INOUT | TILE_FULL | AFFINITY,
                                    PARSEC_DTD_ARG_END);
    break;
  case 4:
    parsec_dtd_taskpool_insert_task(dtd_tp, test_task4,    0,  "test_task4",
                                    sizeof(int),    &i,  VALUE,
                                    sizeof(int),    &j,  VALUE,
                                    PASSED_BY_REF,  args[1], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[2], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[3], INPUT | TILE_FULL,
                                    PASSED_BY_REF,  args[0], INOUT | TILE_FULL | AFFINITY,
                                    PARSEC_DTD_ARG_END);
    break;
  default:
    assert(false && "unexpected num_args");
  };
}

ParsecApp::ParsecApp(int argc, char **argv)
  : App(argc, argv)
{
  printf("init parsec\n");
  
  int Cseed = 0;

  /* Set defaults for non argv iparams */
  iparam_default_gemm(iparam);
  iparam_default_ibnbmb(iparam, 0, 1, 1);
#if defined(HAVE_CUDA) && 1
  iparam[IPARAM_NGPUS] = 0;
#endif
  
  TaskGraph &graph = graphs[0];
  
  N = graph.max_width;
  M = N;
  graph.timesteps = N;
  
  /* Initialize PaRSEC */
  parsec = setup_parsec(argc, argv, iparam);
  PASTE_CODE_IPARAM_LOCALS(iparam);

  LDA = max(LDA, max(M, K));
  LDB = max(LDB, max(K, N));
  LDC = max(LDC, M);
  
  two_dim_block_cyclic_init(&dcC, matrix_RealDouble, matrix_Tile,
                             nodes, rank, MB, NB, LDC, N, 0, 0,
                             M, N, SMB, SNB, P);

  dcC.mat = parsec_data_allocate((size_t)dcC.super.nb_local_tiles * \
                                 (size_t)dcC.super.bsiz *      \
                                 (size_t)parsec_datadist_getsizeoftype(dcC.super.mtype)); \
  parsec_data_collection_set_key((parsec_data_collection_t*)&dcC, "dcC"); 
  

  /* Initializing dc for dtd */
  __dcC = &dcC;
  parsec_dtd_data_collection_init((parsec_data_collection_t *)&dcC);



  /* Getting new parsec handle of dtd type */
  dtd_tp = parsec_dtd_taskpool_new();

  /* Default type */
  dplasma_add2arena_tile( parsec_dtd_arenas[TILE_FULL],
                          dcC.super.mb*dcC.super.nb*sizeof(double),
                          PARSEC_ARENA_ALIGNMENT_SSE,
                          parsec_datatype_double_t, dcC.super.mb );

  /* matrix generation */
  dplasma_dplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcC, Cseed);


  parsec_context_add_taskpool( parsec, dtd_tp );
}

ParsecApp::~ParsecApp()
{
  printf("clean up parsec\n");
  /* Cleaning up the parsec handle */
  parsec_taskpool_free( dtd_tp );

  /* Cleaning data arrays we allocated for communication */
  parsec_matrix_del2arena( parsec_dtd_arenas[0] );


  /* Cleaning data arrays we allocated for communication */
  parsec_dtd_data_collection_fini( (parsec_data_collection_t *)&dcC );

  parsec_data_free(dcC.mat);
  parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcC);

  cleanup_parsec(parsec, iparam);
}

void ParsecApp::execute_main_loop()
{
  
  display();
  
  /* #### parsec context Starting #### */
  PASTE_CODE_FLOPS(FLOPS_DGEMM, ((DagDouble_t)M,(DagDouble_t)N,(DagDouble_t)K));
  SYNC_TIME_START();
  /* start parsec context */
  parsec_context_start(parsec);
  int i, j;
  
  int x, y;
  
  for (y = 0; y < M; y++) {
    execute_timestep(y);
  }
  /*
  printf("start insert task\n");
  std::vector<parsec_dtd_tile_t*> args;
  int num_args = 0;
  for (i = 0; i < M; i++) {
      for (j = 0; j < N; j++ ) {
          if (i == 0) {
            args.clear();
            num_args = 1;
            args.push_back(TILE_OF(C, 0, j));
          } else if (i != 0 && j == 0) {
            num_args = 3;
            args.clear();
            args.push_back(TILE_OF(C, i, j));
            args.push_back(TILE_OF(C, i-1, 0));
            args.push_back(TILE_OF(C, i-1, 1));
          } else if (i != 0 && j == (N-1)) {
            num_args = 3;
            args.clear();
            args.push_back(TILE_OF(C, i, j));
            args.push_back(TILE_OF(C, i-1, N-2));
            args.push_back(TILE_OF(C, i-1, N-1));
          } else {
            num_args = 4;
            args.clear();
            args.push_back(TILE_OF(C, i, j));
            args.push_back(TILE_OF(C, i-1, j-1));
            args.push_back(TILE_OF(C, i-1, j));
            args.push_back(TILE_OF(C, i-1, j+1));
          }
          insert_task(num_args, i, j, args);
          
      }
  }*/
  

  parsec_dtd_data_flush_all( dtd_tp, (parsec_data_collection_t *)&dcC );

  /* finishing all the tasks inserted, but not finishing the handle */
  parsec_dtd_taskpool_wait( parsec, dtd_tp );

  /* Waiting on all handle and turning everything off for this context */
  parsec_context_wait( parsec );

  /* #### PaRSEC context is done #### */

  SYNC_TIME_PRINT(rank, ("\tPxQ= %3d %-3d NB= %4d N= %7d : %14f gflops\n",
                         P, Q, NB, N,
                         gflops=(flops/1e9)/sync_time_elapsed));

}

void ParsecApp::execute_timestep(long t)
{
  const TaskGraph &g = graphs[0];
  long offset = g.offset_at_timestep(t);
  long width = g.width_at_timestep(t);
  long dset = g.dependence_set_at_timestep(t);
  
  std::vector<parsec_dtd_tile_t*> args;
  
  printf("ts %d, offset %d, width %d, offset+width-1 %d ", t, offset, width, offset+width-1);
  for (int x = offset; x <= offset+width-1; x++) {
    std::vector<std::pair<long, long> > deps = g.dependencies(dset, x);
    std::pair<long, long> dep = deps[0];
    assert(deps.size() == 1);
    int num_args = dep.second - dep.first + 1;
    printf("%d[%d, %d, %d] ", x, num_args, dep.first, dep.second);
    
    if (t == 0) {
      num_args = 1;
      args.clear();
      args.push_back(TILE_OF(C, t, x)); 
    } else {
      num_args ++;
      args.clear();
      args.push_back(TILE_OF(C, t, x));
      for (int i = dep.first; i <= dep.second; i++) {
        args.push_back(TILE_OF(C, t-1, i));  
      }
    }
    insert_task(num_args, t, x, args); 
  }
  printf("\n");
}

int main(int argc, char ** argv)
{
    
    ParsecApp app(argc, argv);
    app.execute_main_loop();

    return 0;
}