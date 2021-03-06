#include <cstring>
#include "pmf.h"
#include <sys/stat.h>
#include <sys/types.h>

smat_t::format_t file_fmt = smat_t::TXT;
bool do_shuffle = true;

void exit_with_help()
{
	printf(
	"Usage: omp-pmf-train [options] data_dir weight_dir output_folder_name [model_filename]\n"
	"options:\n"
	"    -s type : set type of solver (default 0)\n"
	"    	 0 -- CCDR1 with fundec stopping condition\n"
	"    	 1 -- ALS\n"
	"    	 2 -- SGD\n"
	"    	 9 -- CCDR1 with adaptive ranking increasing\n"
	"    	10 -- PU-CCDR1\n"
	"    	11 -- PU-ALS\n"
	"    	12 -- PU-SGD\n"
	"    	22 -- PU-SGD-ORIG\n"
	"    -k rank : set the rank (default 10)\n"
	"    -n threads : set the number of threads (default 4)\n"
	"    -l lambda : set the regularization parameter lambda (default 0.1)\n"
	"    -r rho : set the parameter rho for PU formulation (default 0.01)\n"
	"    -P pu_type: set the types of SGD for PU formulation (default 2)\n"
	"        2 -- PU2 (SG with reformulation):\n"
	"        6 -- PU6 (SG with binary search):\n"
	"    -t max_iter: set the number of iterations (default 5)\n"
	"    -T max_iter: set the number of inner iterations used in CCDR1 (default 5)\n"
	"    -e epsilon : set inner termination criterion epsilon of CCDR1 (default 1e-3)\n"
	"    -p do_predict: do prediction or not (default 0)\n"
	"    -q verbose: show information or not (default 0)\n"
	"    -N do_nmf: do nmf (default 0)\n"
	"    -S shuffle: random shuffle for rows and columns (default 1)\n"
        "    -E save_each: save word embedding in each iteration (default 0)\n"
	//"    -w warm_start: warm start or not for CCDR1 (default 0)\n"
	"    -b remove_bias: remove bias or not (default 1)\n"
        "    -X x_max: using for implementing glove's  weight (default 10)\n"
        "    -W implement glove weight: implement glove's  weight (default 0)\n"
        "    -G implement glove bias: add bias for each column and row (default 0)\n"
        "        !!!this is important, please note\n"
        "        0 --do not implement glove bias\n"
        "        1 --implement glove bias, but save the result without bias term\n"
        "        2 --implement glove bias, and save the result with bias term and 1\n"
	"    -f format: select input format (default 0)\n"
	"        0 -- plaintxt format\n"
	"        1 -- PETSc format\n"
	);
	exit(1);
}

pmf_parameter_t parse_command_line(int argc, char **argv, char *input_file_name, char *count_file_name, char* output_folder_name, char *model_file_name)
{
	pmf_parameter_t param;   
	int i;

	// parse options
	for(i=1;i<argc;i++)
	{
		if(argv[i][0] != '-') break;
		if(++i>=argc)
			exit_with_help();
		switch(argv[i-1][1])
		{
			case 's':
				param.solver_type = atoi(argv[i]);
				break;
                        case 'X':
				param.x_max = atoi(argv[i]);
				break;

			case 'W':
				param.glove_weight = atoi(argv[i]);
				break;

	        	case 'G':
				param.glove_bias = atoi(argv[i]);
				break;

	        	case 'k':
				param.k = atoi(argv[i]);
				break;

			case 'n':
				param.threads = atoi(argv[i]);
				break;

			case 'l':
				param.lambda = atof(argv[i]);
				break;

			case 'r':
				param.rho = atof(argv[i]);
				break;

			case 't':
				param.maxiter = atoi(argv[i]);
				break;

			case 'T':
				param.maxinneriter = atoi(argv[i]);
				break;

			case 'e':
				param.eps = atof(argv[i]);
				param.eta0 = atof(argv[i]);
				break;

			case 'B':
				param.nr_blocks = atoi(argv[i]);
				break;

			case 'm':
				param.lrate_method = atoi(argv[i]);
				break;

			case 'u':
				param.betaup = atof(argv[i]);
				break;

			case 'd':
				param.betadown = atof(argv[i]);
				break;

			case 'w':
				param.warm_start = atof(argv[i]);
				break;

			case 'p':
				param.do_predict = atoi(argv[i]);
				break;

			case 'P':
				param.pu_type = atoi(argv[i]);
				break;


			case 'q':
                                param.verbose = atoi(argv[i]);
				break;

			case 'N':
				param.do_nmf = atoi(argv[i]) == 1? true : false;
				break;

			case 'S':
				do_shuffle = atoi(argv[i]);
				break;

			case 'b':
				param.remove_bias = atof(argv[i]);
				break;
                        case 'E':
                                param.save_each = atoi(argv[i]);
                                break;

			case 'f':
				file_fmt = (smat_t::format_t) atoi(argv[i]);
				break;

			default:
				fprintf(stderr,"unknown option: -%c\n", argv[i-1][1]);
				exit_with_help();
				break;
		}
	}

	if (param.do_predict != 0 && param.verbose == 0)
		param.verbose = 1;
        if (param.glove_bias != 0) {param.k += 2;printf("need glove bias, k is %d\n", param.k);}

	if(i>=argc)
		exit_with_help();

	strcpy(input_file_name, argv[i]);
        
        i++;
	strcpy(count_file_name, argv[i]);
	
        i++;
	strcpy(output_folder_name, argv[i]);
	
        if(i<argc-1)
		strcpy(model_file_name,argv[i+1]);
	else
	{
		char *p = argv[i]+ strlen(argv[i])-1;
		while (*p == '/')
			*p-- = 0;
		p = strrchr(argv[i],'/');
		if(p==NULL)
			p = argv[i];
		else
			++p;
		sprintf(model_file_name,"%s.model",p);
	}
	return param;
}


void run_ccdr1(pmf_parameter_t &param, const char *input_file_name, const char *count_file_name, const char *model_file_name=NULL) { // {{{
	FILE *model_fpw = NULL;	
	FILE *model_fph = NULL;//FIXIT

        float rho = param.rho;
        float lambda = param.lambda;
        int maxiter = param.maxiter;
        
      	if(model_file_name) {
		char matrixname[1024];

                sprintf(matrixname, "%s.iter%d.final.words", model_file_name, maxiter);
		model_fpw = fopen(matrixname, "w");
                if(model_fpw == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}	

                sprintf(matrixname, "%s.iter%d.final.contexts", model_file_name, maxiter);
                model_fph = fopen(matrixname, "w");
		if(model_fph == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}
	}

	smat_t training_set, test_set;
	pmf_read_data(input_file_name, training_set, test_set, file_fmt);
        if (param.glove_weight){
        printf("now implementing glove weight\n");
        int x_max = param.x_max;
        float  alpha = 0.75;
        printf("x_max is %d\n", x_max);
        smat_t count_training_set, count_test_set;
	pmf_read_data(count_file_name,count_training_set, count_test_set, file_fmt);
        
        for(size_t idx=0; idx < training_set.nnz; idx++){
        training_set.weight[idx] = (count_training_set.val[idx]>x_max)?1:pow(count_training_set.val[idx]/x_max, alpha);
        training_set.weight_t[idx] = (count_training_set.val_t[idx]>x_max)?1:pow(count_training_set.val_t[idx]/x_max, alpha);
        }

        for(size_t idx=0; idx < test_set.nnz; idx++){
        test_set.weight[idx] =  (count_test_set.val[idx]>x_max)?1:pow(count_test_set.val[idx]/x_max, alpha);
        test_set.weight_t[idx] = (count_test_set.val_t[idx]>x_max)?1:pow(count_test_set.val_t[idx]/x_max, alpha);
        }
        }
	pmf_model_t model(training_set.rows, training_set.cols, param.k, pmf_model_t::COLMAJOR);

    
	// Random permutation for rows and cols of R for better load balancing

	std::vector<unsigned> row_perm, inverse_row_perm;
	std::vector<unsigned> col_perm, inverse_col_perm;
	if(do_shuffle) {
		gen_permutation_pair(training_set.rows, row_perm, inverse_row_perm);
		gen_permutation_pair(training_set.cols, col_perm, inverse_col_perm);

		training_set.apply_permutation(row_perm, col_perm);
		test_set.apply_permutation(row_perm, col_perm);
	}

	puts("starts!");
	double time = omp_get_wtime();
	if(param.solver_type == CCDR1)
		ccdr1(training_set, test_set, param, model);
	else if(param.solver_type == CCDR1_SPEEDUP)
		ccdr1_speedup(training_set, test_set, param, model);
	
        if(param.solver_type == PU_CCDR1)
		ccdr1_pu(training_set, test_set, param, model, do_shuffle, row_perm, col_perm, inverse_row_perm, inverse_col_perm, model_file_name);
	printf("Wall-time: %lg secs\n", omp_get_wtime() - time);

	if(model_fpw) {
		if(do_shuffle)
			model.apply_permutation(row_perm, col_perm);
		model.save_embedding(model_fpw,model_fph, param.glove_bias);//FIXIT
		fclose(model_fpw);
                fclose(model_fph);
                
	}

	return ;
} // }}}

void run_als(pmf_parameter_t &param, const char *input_file_name, const char *model_file_name=NULL) { // {{{
	FILE *model_fpw = NULL;
        FILE *model_fph = NULL;
        
        if(model_file_name) {
        	char matrixname[1024];
                sprintf(matrixname, "%s.W", model_file_name);

		model_fpw = fopen(matrixname, "w");
                if(model_fpw == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}	

                sprintf(matrixname, "%s.H", model_file_name);


                model_fph = fopen(matrixname, "w");
		if(model_fph == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}

	}
	
        smat_t training_set, test_set;
	pmf_read_data(input_file_name, training_set, test_set, file_fmt);
	// ALS requires rowmajor model
	pmf_model_t model(training_set.rows, training_set.cols, param.k, pmf_model_t::ROWMAJOR);

	// Random permutation for rows and cols of training_set for better load balancing
	std::vector<unsigned> row_perm, inverse_row_perm;
	std::vector<unsigned> col_perm, inverse_col_perm;
	if(do_shuffle) {
		gen_permutation_pair(training_set.rows, row_perm, inverse_row_perm);
		gen_permutation_pair(training_set.cols, col_perm, inverse_col_perm);

		training_set.apply_permutation(row_perm, col_perm);
		test_set.apply_permutation(row_perm, col_perm);
	}

	puts("starts!");
	double time = omp_get_wtime();
	if(param.solver_type == ALS)
		als(training_set, test_set, param, model);
	else if(param.solver_type == PU_ALS)
		als_pu(training_set, test_set, param, model);
	printf("Wall-time: %lg secs\n", omp_get_wtime() - time);

	if(model_fpw) {
		if(do_shuffle)
			model.apply_permutation(row_perm, col_perm);
		model.save_embedding(model_fpw, model_fph);
		fclose(model_fpw);
                fclose(model_fph);
	}
	return ;
} // }}}

void run_sgd(pmf_parameter_t &param, const char *input_file_name, const char *model_file_name=NULL) { // {{{
	FILE *model_fpw = NULL;
        FILE *model_fph = NULL;
        


        
        if(model_file_name) {
        	char matrixname[1024];
                sprintf(matrixname, "%s.W", model_file_name);

		model_fpw = fopen(matrixname, "w");
                if(model_fpw == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}	

                sprintf(matrixname, "%s.H", model_file_name);


                model_fph = fopen(matrixname, "w");
		if(model_fph == NULL) {
			fprintf(stderr,"Error: can't open model file %s\n", model_file_name);
			exit(1);
		}

	}
	
	blocks_t training_set, test_set;
	pmf_read_data(input_file_name, training_set, test_set);
	// SGD requires rowmajor model
	pmf_model_t model(training_set.rows, training_set.cols, param.k, pmf_model_t::ROWMAJOR);

	// Random permutation for rows and cols of training_set for better load balancing
	std::vector<unsigned> row_perm, inverse_row_perm;
	std::vector<unsigned> col_perm, inverse_col_perm;
	if(do_shuffle) {
		gen_permutation_pair(training_set.rows, row_perm, inverse_row_perm);
		gen_permutation_pair(training_set.cols, col_perm, inverse_col_perm);

		training_set.apply_permutation(row_perm, col_perm);
		test_set.apply_permutation(row_perm, col_perm);
	}

	puts("starts!");
	double time = omp_get_wtime();
	if(param.solver_type == SGD || param.solver_type == PU_SGD_ORIG)
		sgd(training_set, test_set, param, model);
	else if(param.solver_type == PU_SGD)
		sgd_pu(training_set, test_set, param, model);
	printf("Wall-time: %lg secs\n", omp_get_wtime() - time);

	if(model_fpw) {
		if(do_shuffle)
			model.apply_permutation(row_perm, col_perm);
		model.save_embedding(model_fpw, model_fph);
		fclose(model_fpw);
                fclose(model_fph);
	}
	return ;
} // }}}









int main(int argc, char* argv[]){
        
	char input_file_name[1024];
	char count_file_name[1024];
        char output_folder_name[1024];
	char model_file_name[1024];
	char model_file_name_tmp[1024];
	pmf_parameter_t param = parse_command_line(argc, argv, input_file_name, count_file_name, output_folder_name, model_file_name);
        int result = mkdir(output_folder_name, 0777);
        sprintf(model_file_name_tmp,"%s/%s",output_folder_name, model_file_name);
        sprintf(model_file_name,"%s", model_file_name_tmp);
        switch (param.solver_type){
		case CCDR1:
		case CCDR1_SPEEDUP:
		case PU_CCDR1_SPEEDUP:
		case PU_CCDR1:
			run_ccdr1(param, input_file_name, count_file_name, model_file_name);
			break;
		case ALS:
		case PU_ALS:
			run_als(param, input_file_name, model_file_name);
			break;
		case SGD:
		case PU_SGD:
		case PU_SGD_ORIG:
			run_sgd(param, input_file_name, model_file_name);
			break;
		default:
			fprintf(stderr, "Error: wrong solver type (%d)!\n", param.solver_type);
			break;
	}
	return 0;
}

