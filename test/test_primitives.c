/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/*
 * Uncomment this definition to disable the OPA library and instead use naive
 * (non-atomic) operations.  This should cause failures.
 */
/*
#define OPA_TEST_NAIVE
*/

#include "opa_test.h"

/* Definitions for test_threaded_loadstore_int */
/* LOADSTORE_INT_DIFF is the difference between the unique values of successive
 * threads.  This value contains 0's in the top half of the int and 1's in the
 * bottom half.  Therefore, for 4 byte ints the sequence of unique values is:
 * 0x00000000, 0x0000FFFF, 0x0001FFFE, 0x0002FFFD, etc. */
#define LOADSTORE_INT_DIFF ((1 << (sizeof(int) * CHAR_BIT / 2)) - 1)
#define LOADSTORE_INT_NITER (4000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being read/written by all threads */
    int         unique_val;     /* This thread's unique value to store in shared_val */
    int         nerrors;        /* Number of errors */
    int         master_thread;  /* Whether this is the master thread */
} loadstore_int_t;

/* Definitions for test_threaded_loadstore_ptr */
#define LOADSTORE_PTR_DIFF (((unsigned long) 1 << (sizeof(void *) * CHAR_BIT / 2)) - 1)
#define LOADSTORE_PTR_NITER (4000000 / iter_reduction[curr_test])
typedef struct {
    OPA_ptr_t   *shared_val;    /* Shared int being read/written by all threads */
    void        *unique_val;    /* This thread's unique value to store in shared_val */
    int         nerrors;        /* Number of errors */
    int         master_thread;  /* Whether this is the master thread */
} loadstore_ptr_t;

/* Definitions for test_threaded_add */
#define ADD_EXPECTED ADD_NITER
#define ADD_NITER (2000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being added to by all threads */
    int         unique_val;     /* This thread's unique value to add to shared_val */
    int         master_thread;  /* Whether this is the master thread */
} add_t;

/* Definitions for test_threaded_incr_decr */
#define INCR_DECR_EXPECTED INCR_DECR_NITER
#define INCR_DECR_NITER (2000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being manipulated by all threads */
    int         master_thread;  /* Whether this is the master thread */
} incr_decr_t;

/* Definitions for test_threaded_decr_and_test */
#define DECR_AND_TEST_NITER_INNER 20            /* *Must* be even */
#define DECR_AND_TEST_NITER_OUTER (10000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being decr and tested by all threads */
    unsigned    ntrue;          /* # of times decr_and_test returned true */
    int         master_thread;  /* Whether this is the master thread */
} decr_test_t;

/* Definitions for test_threaded_faa */
/* Uses definitions from test_threaded_add */

/* Definitions for test_threaded_faa_ret */
#define FAA_RET_EXPECTED (-((nthreads - 1) * FAA_RET_NITER))
#define FAA_RET_NITER (2000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being added to by all threads */
    int         nerrors;        /* Number of errors */
    int         n1;             /* # of times faa returned 1 */
    int         master_thread;  /* Whether this is the master thread */
} faa_ret_t;

/* Definitions for test_threaded_fai_fad */
/* Uses definitions from test_threaded_incr_decr */

/* Definitions for test_threaded_fai_ret */
#define FAI_RET_EXPECTED ((nthreads - 1) * FAA_RET_NITER)
#define FAI_RET_NITER (2000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being added to by all threads */
    int         nerrors;        /* Number of errors */
    int         nm1;            /* # of times faa returned -1 */
    int         master_thread;  /* Whether this is the master thread */
} fai_ret_t;

/* Definitions for test_threaded_fad_red */
/* Uses definitions from test_threaded_faa_ret */

/* Definitions for test_threaded_cas_int */
#define CAS_INT_NITER (5000000 / iter_reduction[curr_test])
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being added to by all threads */
    int         threadno;       /* Unique thread number */
    int         nsuccess;       /* # of times cas succeeded */
    int         master_thread;  /* Whether this is the master thread */
} cas_int_t;

/* Definitions for test_threaded_cas_ptr */
#define CAS_PTR_NITER (5000000 / iter_reduction[curr_test])
typedef struct {
    OPA_ptr_t   *shared_val;    /* Shared ptr being added to by all threads */
    int         *threadno;      /* Unique thread number */
    int         *max_threadno;  /* Maximum unique thread number */
    int         nsuccess;       /* # of times cas succeeded */
    int         master_thread;  /* Whether this is the master thread */
} cas_ptr_t;

/* Definitions for test_grouped_cas_int */
#define GROUPED_CAS_INT_NITER (5000000 / iter_reduction[curr_test])
#define GROUPED_CAS_INT_TPG 4
typedef struct {
    OPA_int_t   *shared_val;    /* Shared int being added to by all threads */
    int         groupno;        /* Unique group number */
    int         ngroups;        /* Number of groups */
    int         nsuccess;       /* # of times cas succeeded */
    int         master_thread;  /* Whether this is the master thread */
} grouped_cas_int_t;

/* Definitions for test_grouped_cas_ptr */
#define GROUPED_CAS_PTR_NITER (5000000 / iter_reduction[curr_test])
#define GROUPED_CAS_PTR_TPG 4
typedef struct {
    OPA_ptr_t   *shared_val;    /* Shared int being added to by all threads */
    int         *groupno;       /* Unique group number */
    int         *max_groupno;   /* Maximum unique group number */
    int         nsuccess;       /* # of times cas succeeded */
    int         master_thread;  /* Whether this is the master thread */
} grouped_cas_ptr_t;


/*-------------------------------------------------------------------------
 * Function: test_simple_loadstore_int
 *
 * Purpose: Tests basic functionality of OPA_load_int and OPA_store_int with a
 *          single thread.  Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 19, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_loadstore_int(void)
{
    OPA_int_t   a, b;

    TESTING("simple integer load/store functionality", 0);

    /* Store 0 in a, -1 in b.  Verify that these values are returned by
     * OPA_load_int. */
    OPA_store_int(&a, 0);
    if(0 != OPA_load_int(&a)) TEST_ERROR;
    OPA_store_int(&b, -1);
    if(-1 != OPA_load_int(&b)) TEST_ERROR;
    if(0 != OPA_load_int(&a)) TEST_ERROR;
    if(-1 != OPA_load_int(&b)) TEST_ERROR;

    /* Store INT_MIN in a and INT_MAX in b.  Verify that these values are
     * returned by OPA_load_int. */
    OPA_store_int(&a, INT_MIN);
    if(INT_MIN != OPA_load_int(&a)) TEST_ERROR;
    OPA_store_int(&b, INT_MAX);
    if(INT_MAX != OPA_load_int(&b)) TEST_ERROR;
    if(INT_MIN != OPA_load_int(&a)) TEST_ERROR;
    if(INT_MAX != OPA_load_int(&b)) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_loadstore_int() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_loadstore_int_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_loadstore_int
 *
 * Return: Success: NULL
 *         Failure: non-NULL
 *
 * Programmer: Neil Fortner
 *             Thursday, March 19, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_loadstore_int_helper(void *_udata)
{
    loadstore_int_t     *udata = (loadstore_int_t *)_udata;
    int                 loaded_val;
    unsigned            niter = LOADSTORE_INT_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++) {
        /* Store the unique value into the shared value */
        OPA_store_int(udata->shared_val, udata->unique_val);

        /* Load the shared_value, and check if it is valid */
        if((loaded_val = OPA_load_int(udata->shared_val)) % LOADSTORE_INT_DIFF) {
            printf("    Unexpected load: %d is not a multiple of %d\n",
                    loaded_val, LOADSTORE_INT_DIFF);
            udata->nerrors++;
        } /* end if */
    } /* end for */

    /* Any non-NULL exit value indicates an error, we use &i here */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_loadstore_int_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_loadstore_int
 *
 * Purpose: Tests atomicity of OPA_load_int and OPA_store_int.  Launches nthreads
 *          threads, each of which repeatedly loads and stores a shared
 *          variable.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 19, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_loadstore_int(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    loadstore_int_t     *thread_data = NULL; /* User data structs for each thread */
    OPA_int_t           shared_int;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            nerrors = 0;    /* number of errors */
    unsigned            i;

    TESTING("integer load/store", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (loadstore_int_t *) calloc(nthreads,
            sizeof(loadstore_int_t)))) TEST_ERROR;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Initialize thread data structs */
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_int;
        thread_data[i].unique_val = i * LOADSTORE_INT_DIFF;;
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_loadstore_int_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_loadstore_int_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Check for errors */
    for(i=0; i<nthreads; i++)
        nerrors += thread_data[i].nerrors;
    if(nerrors)
        FAIL_OP_ERROR(printf("    %d unexpected return%s from OPA_load_int\n", nerrors,
                nerrors == 1 ? "" : "s"));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("integer load/store", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_loadstore_int() */


/*-------------------------------------------------------------------------
 * Function: test_simple_loadstore_ptr
 *
 * Purpose: Tests basic functionality of OPA_load_int and OPA_store_int with a
 *          single thread.  Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_loadstore_ptr(void)
{
    OPA_ptr_t   a, b;

    TESTING("simple pointer load/store functionality", 0);

    /* Store 0 in a, 1 in b.  Verify that these values are returned by
     * OPA_load_int. */
    OPA_store_ptr(&a, (void *) 0);
    if((void *) 0 != OPA_load_ptr(&a)) TEST_ERROR;
    OPA_store_ptr(&b, (void *) 1);
    if((void *) 1 != OPA_load_ptr(&b)) TEST_ERROR;
    if((void *) 0 != OPA_load_ptr(&a)) TEST_ERROR;
    if((void *) 1 != OPA_load_ptr(&b)) TEST_ERROR;

    /* Store -1 in a and -2 in b.  Verify that these values are returned by
     * OPA_load_int. */
    OPA_store_ptr(&a, (void *) -2);
    if((void *) -2 != OPA_load_ptr(&a)) TEST_ERROR;
    OPA_store_ptr(&b, (void *) -1);
    if((void *) -1 != OPA_load_ptr(&b)) TEST_ERROR;
    if((void *) -2 != OPA_load_ptr(&a)) TEST_ERROR;
    if((void *) -1 != OPA_load_ptr(&b)) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_loadstore_ptr() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_loadstore_ptr_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_loadstore_ptr
 *
 * Return: Success: NULL
 *         Failure: non-NULL
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_loadstore_ptr_helper(void *_udata)
{
    loadstore_ptr_t     *udata = (loadstore_ptr_t *)_udata;
    unsigned long       loaded_val;
    int                 niter = LOADSTORE_PTR_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++) {
        /* Store the unique value into the shared value */
        OPA_store_ptr(udata->shared_val, udata->unique_val);

        /* Load the shared_value, and check if it is valid */
        if((loaded_val = (unsigned long) OPA_load_ptr(udata->shared_val)) % LOADSTORE_PTR_DIFF) {
            printf("    Unexpected load: %lu is not a multiple of %lu\n",
                    loaded_val, LOADSTORE_PTR_DIFF);
            udata->nerrors++;
        } /* end if */
    } /* end for */

    /* Any non-NULL exit value indicates an error, we use &i here */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_loadstore_ptr_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_loadstore_ptr
 *
 * Purpose: Tests atomicity of OPA_load_int and OPA_store_int.  Launches nthreads
 *          threads, each of which repeatedly loads and stores a shared
 *          variable.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_loadstore_ptr(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    loadstore_ptr_t     *thread_data = NULL; /* User data structs for each thread */
    OPA_ptr_t           shared_ptr;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            nerrors = 0;    /* number of errors */
    unsigned            i;

    TESTING("pointer load/store", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (loadstore_ptr_t *) calloc(nthreads,
            sizeof(loadstore_ptr_t)))) TEST_ERROR;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Initialize thread data structs */
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_ptr;
        thread_data[i].unique_val = (void *) ((unsigned long) i * LOADSTORE_PTR_DIFF);
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_loadstore_ptr_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_loadstore_ptr_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;


    /* Check for errors */
    for(i=0; i<nthreads; i++)
        nerrors += thread_data[i].nerrors;
    if(nerrors)
        FAIL_OP_ERROR(printf("    %d unexpected return%s from OPA_load_ptr\n", nerrors,
                nerrors == 1 ? "" : "s"));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("pointer load/store", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_loadstore_ptr() */


/*-------------------------------------------------------------------------
 * Function: test_simple_add_incr_decr
 *
 * Purpose: Tests basic functionality of OPA_add_int, OPA_incr_int and
 *          OPA_decr_int with a single thread.  Does not test atomicity
 *          of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_add_incr_decr(void)
{
    OPA_int_t   a;


    TESTING("simple add/incr/decr functionality", 0);

    /* Store 0 in a */
    OPA_store_int(&a, 0);

    /* Add INT_MIN */
    OPA_add_int(&a, INT_MIN);

    /* Increment */
    OPA_incr_int(&a);

    /* Add INT_MAX */
    OPA_add_int(&a, INT_MAX);

    /* Decrement */
    OPA_decr_int(&a);

    /* Load the result, verify it is correct */
    if(OPA_load_int(&a) != INT_MIN + 1 + INT_MAX - 1) TEST_ERROR;

    /* Store 0 in a */
    OPA_store_int(&a, 0);

    /* Add INT_MAX */
    OPA_add_int(&a, INT_MAX);

    /* Decrement */
    OPA_decr_int(&a);

    /* Add INT_MIN */
    OPA_add_int(&a, INT_MIN);

    /* Increment */
    OPA_incr_int(&a);

    /* Load the result, verify it is correct */
    if(OPA_load_int(&a) != INT_MAX - 1 + INT_MIN + 1) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_add_incr_decr() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_add_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_add
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_add_helper(void *_udata)
{
    add_t               *udata = (add_t *)_udata;
    unsigned            niter = ADD_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Add the unique value to the shared value */
        OPA_add_int(udata->shared_val, udata->unique_val);

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_add_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_add
 *
 * Purpose: Tests atomicity of OPA_add_int.  Launches nthreads threads
 *          each of which repeatedly adds a unique number to a shared
 *          variable.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, March 20, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_add(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    add_t               *thread_data = NULL; /* User data structs for each thread */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("add", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (add_t *) calloc(nthreads, sizeof(add_t))))
        TEST_ERROR;

    /* Initialize thread data structs.  All the unique values must add up to
     * 0. */
    OPA_store_int(&shared_val, 0);
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].unique_val = i - (nthreads - 1) / 2
                - (!(nthreads % 2) && (i >= nthreads / 2))
                + (i == nthreads - 1);
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_add_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_add_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != ADD_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), ADD_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("add", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_add() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_incr_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_incr_decr
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Tuesday, April 21, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_incr_helper(void *_udata)
{
    incr_decr_t         *udata = (incr_decr_t *)_udata;
    unsigned            niter = INCR_DECR_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Increment the shared value */
        OPA_incr_int(udata->shared_val);

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_incr_helper() */


/*-------------------------------------------------------------------------
 * Function: threaded_decr_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_incr_decr
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Tuesday, April 21, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_decr_helper(void *_udata)
{
    incr_decr_t         *udata = (incr_decr_t *)_udata;
    unsigned            niter = INCR_DECR_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Decrement the shared value */
        OPA_decr_int(udata->shared_val);

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_decr_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_incr_decr
 *
 * Purpose: Tests atomicity of OPA_incr_int and OPA_decr_int.  Launches
 *          nthreads threads, each of which repeatedly either increments
 *          or decrements a shared variable.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, April 21, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_incr_decr(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    incr_decr_t         *thread_data = NULL; /* User data structs for each thread */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    /* Must use an odd number of threads */
    if(!(nthreads & 1))
        nthreads--;

    TESTING("incr and decr", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc(nthreads * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (incr_decr_t *) calloc(nthreads, sizeof(incr_decr_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, 0);
    for(i=0; i<nthreads; i++)
        thread_data[i].shared_val = &shared_val;
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Set the initial state of the shared value (0) */
    OPA_store_int(&shared_val, 0);

    /* Create the threads.  All the unique values must add up to 0. */
    for(i=0; i<(nthreads - 1); i++) {
        if(i & 1) {
            if(pthread_create(&threads[i], &ptattr, threaded_decr_helper,
                    &thread_data[i])) TEST_ERROR;
        } else
            if(pthread_create(&threads[i], &ptattr, threaded_incr_helper,
                    &thread_data[i])) TEST_ERROR;
    } /* end for */
    if(i & 1)
        (void)threaded_decr_helper(&thread_data[i]);
    else
        (void)threaded_incr_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != INCR_DECR_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), INCR_DECR_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("incr and decr", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_incr_decr() */


/*-------------------------------------------------------------------------
 * Function: test_simple_decr_and_test
 *
 * Purpose: Tests basic functionality of OPA_decr_and_test_int with a
 *          single thread.  Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Wednesday, April 22, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_decr_and_test(void)
{
    OPA_int_t   a;
    int         i;

    TESTING("simple decr and test functionality", 0);

    /* Store 10 in a */
    OPA_store_int(&a, 10);

    for(i = 0; i<20; i++)
        if(OPA_decr_and_test_int(&a))
            if(i != 9) TEST_ERROR;

    if(OPA_load_int(&a) != -10) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_decr_and_test() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_decr_and_test_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_decr_and_test
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Wednesday, April 22, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_decr_and_test_helper(void *_udata)
{
    decr_test_t         *udata = (decr_test_t *)_udata;
    unsigned            i;

    /* Main loop */
    for(i=0; i<DECR_AND_TEST_NITER_INNER; i++)
        /* Add the unique value to the shared value */
        if(OPA_decr_and_test_int(udata->shared_val))
            udata->ntrue++;

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_decr_and_test_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_decr_and_test
 *
 * Purpose: Tests atomicity of OPA_decr_and_test_int.  Launches nthreads
 *          threads, each of which repeatedly adds a unique number to a
 *          shared variable.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, April 21, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_decr_and_test(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    decr_test_t         *thread_data = NULL; /* User data structs for each thread */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            ntrue_total;
    int                 starting_val;
    unsigned            niter = DECR_AND_TEST_NITER_OUTER;
    unsigned            nthreads = num_threads[curr_test];
    unsigned            nerrors = 0;
    unsigned            i, j;

    TESTING("decr and test", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (decr_test_t *) calloc(nthreads, sizeof(decr_test_t))))
        TEST_ERROR;

    /* Initialize the "shared_val" and "master thread" fields (ntrue will be
     * initialized in the loop) */
    for(i=0; i<nthreads; i++)
        thread_data[i].shared_val = &shared_val;
    thread_data[nthreads-1].master_thread = 1;

    /* Calculate the starting value for the shared int */
    starting_val = DECR_AND_TEST_NITER_INNER * nthreads / 2;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Outer loop (perform the threaded test multiple times */
    for(i=0; i<niter; i++) {
        /* Initialize thread data structs */
        OPA_store_int(&shared_val, starting_val);
        for(j=0; j<nthreads; j++)
            thread_data[j].ntrue = 0;

        /* Create the threads */
        for(j=0; j<(nthreads - 1); j++)
            if(pthread_create(&threads[j], &ptattr, threaded_decr_and_test_helper,
                    &thread_data[j])) TEST_ERROR;
        (void)threaded_decr_and_test_helper(&thread_data[j]);

        /* Join the threads */
        for (j=0; j<(nthreads - 1); j++)
            if(pthread_join(threads[j], NULL)) TEST_ERROR;

        /* Verify the results */
        ntrue_total = 0;
        for (j=0; j<nthreads; j++) {
            /* Verify that OPA_decr_and_test_int returned true at most once */
            if(thread_data[j].ntrue > 1) {
                printf("\n    Unexpected return from thread %u: OPA_decr_and_test_int returned true %u times",
                        j, thread_data[j].ntrue);
                nerrors++;
            } /* end if */

            /* Update ntrue_total */
            ntrue_total += thread_data[j].ntrue;
        } /* end for */

        /* Verify that OPA_decr_and_test_int returned true exactly once over all
         * the threads. */
        if(ntrue_total != 1) {
            printf("\n    Unexpected result: OPA_decr_and_test_int returned true %u times total",
                    ntrue_total);
            nerrors++;
        } /* end if */

        /* Verify that the shared value contains the expected result */
        if(OPA_load_int(&shared_val) != -starting_val) {
            printf("\n    Unexpected result: %d expected: %d",
                    OPA_load_int(&shared_val), -starting_val);
            nerrors++;
        } /* end if */
    } /* end for */

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    if(nerrors) {
        puts("");
        TEST_ERROR;
    } /* end if */

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("decr and test", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_decr_and_test() */


/*-------------------------------------------------------------------------
 * Function: test_simple_faa_fai_fad
 *
 * Purpose: Tests basic functionality of OPA_fetch_and_add_int,
 *          OPA_fetch_and_incr_int and OPA_fetch_and_decr_int with a 
 *          single thread.  Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, April 23, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_faa_fai_fad(void)
{
    OPA_int_t   a;
    int         result, expected;


    TESTING("simple fetch and add/incr/decr functionality", 0);

    /* Store 0 in a */
    OPA_store_int(&a, 0);
    expected = 0;

    /* Add INT_MIN */
    result = OPA_fetch_and_add_int(&a, INT_MIN);
    if(result != expected) TEST_ERROR;
    expected += INT_MIN;

    /* Increment */
    result = OPA_fetch_and_incr_int(&a);
    if(result != expected) TEST_ERROR;
    expected++;

    /* Add INT_MAX */
    result = OPA_fetch_and_add_int(&a, INT_MAX);
    if(result != expected) TEST_ERROR;
    expected += INT_MAX;

    /* Decrement */
    result = OPA_fetch_and_decr_int(&a);
    if(result != expected) TEST_ERROR;
    expected--;

    /* Load the result, verify it is correct */
    if(OPA_load_int(&a) != INT_MIN + 1 + INT_MAX - 1) TEST_ERROR;

    /* Store 0 in a */
    OPA_store_int(&a, 0);
    expected = 0;

    /* Add INT_MAX */
    result = OPA_fetch_and_add_int(&a, INT_MAX);
    if(result != expected) TEST_ERROR;
    expected += INT_MAX;

    /* Decrement */
    result = OPA_fetch_and_decr_int(&a);
    if(result != expected) TEST_ERROR;
    expected--;

    /* Add INT_MIN */
    result = OPA_fetch_and_add_int(&a, INT_MIN);
    if(result != expected) TEST_ERROR;
    expected += INT_MIN;

    /* Increment */
    result = OPA_fetch_and_incr_int(&a);
    if(result != expected) TEST_ERROR;
    expected++;

    /* Load the result, verify it is correct */
    if(OPA_load_int(&a) != INT_MAX - 1 + INT_MIN + 1) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_faa_fai_fad() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_faa_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_faa
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Thursday, April 23, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_faa_helper(void *_udata)
{
    add_t               *udata = (add_t *)_udata;
    unsigned            niter = ADD_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Add the unique value to the shared value */
        (void) OPA_fetch_and_add_int(udata->shared_val, udata->unique_val);

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_add_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_faa
 *
 * Purpose: Tests atomicity of OPA_fetch_and_add_int.  Launches nthreads
 *          threads, each of which repeatedly adds a unique number to a
 *          shared variable.  Does not test return value of
 *          OPA_fetch_and_add_int.  This is basically a copy of
 *          test_threaded_add.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, April 23, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_faa(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    add_t               *thread_data = NULL; /* User data structs for each thread */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("fetch and add", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (add_t *) calloc(nthreads, sizeof(add_t))))
        TEST_ERROR;

    /* Initialize thread data structs.  All the unique values must add up to
     * 0. */
    OPA_store_int(&shared_val, 0);
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].unique_val = i - (nthreads - 1) / 2
                - (!(nthreads % 2) && (i >= nthreads / 2))
                + (i == nthreads - 1);
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;
    
    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_faa_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_faa_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != ADD_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), ADD_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("fetch and add", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_faa() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_faa_ret_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_faa_ret
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Thursday, April 23, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_faa_ret_helper(void *_udata)
{
    faa_ret_t           *udata = (faa_ret_t *)_udata;
    int                 ret, prev = INT_MAX;
    unsigned            niter = FAA_RET_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++) {
        /* Add -1 to the shared value */
        ret = OPA_fetch_and_add_int(udata->shared_val, -1);

        /* Verify that the value returned is less than the previous return */
        if(ret >= prev) {
            printf("\n    Unexpected return: %d is not less than %d  ", ret, prev);
            (udata->nerrors)++;
        } /* end if */

        /* Check if the return value is 1 */
        if(ret == 1)
            (udata->n1)++;

        /* update prev */
        prev = ret;
    } /* end for */

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_faa_ret_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_faa_ret
 *
 * Purpose: Tests atomicity of OPA_fetch_and_add_int.  Launches nthreads
 *          threads, each of which repeatedly adds -1 to a shared
 *          variable.  Verifies that the value returned is always
 *          decreasing, and that it returns 1 exactly once.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Thursday, April 23, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_faa_ret(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    faa_ret_t           *thread_data = NULL; /* User data structs for threads */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    int                 nerrors = 0;    /* Number of errors */
    int                 n1 = 0;         /* # of times faa returned 1 */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("fetch and add return values", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (faa_ret_t *) calloc(nthreads, sizeof(faa_ret_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, FAA_RET_NITER);
    for(i=0; i<nthreads; i++)
        thread_data[i].shared_val = &shared_val;
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_faa_ret_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_faa_ret_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Count number of errors and number of times 1 was returned */
    for(i=0; i<nthreads; i++) {
        nerrors += thread_data[i].nerrors;
        n1 += thread_data[i].n1;
    } /* end for */

    /* Verify that no errors were reported */
    if(nerrors)
        FAIL_OP_ERROR(printf("    %d unexpected return%s from OPA_fetch_and_add_int\n",
                nerrors, nerrors == 1 ? "" : "s"));

    /* Verify that OPA_fetch_and_add_int returned 1 expactly once */
    if(n1 != 1)
        FAIL_OP_ERROR(printf("    OPA_fetch_and_add_int returned 1 %d times.  Expected: 1\n",
                n1));

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != FAA_RET_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), FAA_RET_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("fetch and add return values", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_faa_ret() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_fai_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_fai_fad
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Friday, May 8, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_fai_helper(void *_shared_val)
{
    OPA_int_t           *shared_val = (OPA_int_t *)_shared_val;
    unsigned            niter = INCR_DECR_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Add the unique value to the shared value */
        (void) OPA_fetch_and_incr_int(shared_val);

    /* Exit */
    pthread_exit(NULL);
} /* end threaded_fai_helper() */


/*-------------------------------------------------------------------------
 * Function: threaded_fad_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_fai_fad
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Friday, May 8, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_fad_helper(void *_shared_val)
{
    OPA_int_t           *shared_val = (OPA_int_t *)_shared_val;
    unsigned            niter = INCR_DECR_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        /* Add the unique value to the shared value */
        (void) OPA_fetch_and_decr_int(shared_val);

    /* Exit */
    pthread_exit(NULL);
} /* end threaded_fad_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_fai_fad
 *
 * Purpose: Tests atomicity of OPA_fetch_and_incr_int and
 *          OPA_fetch_and_decr_int.  Launches nthreads threads, each of
 *          which repeatedly either increments or decrements a shared
 *          variable.  Does not test return values.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Friday, May 8, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_fai_fad(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    /* Must use an odd number of threads */
    if(!(nthreads & 1))
        nthreads--;

    TESTING("fetch and incr/decr", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc(nthreads * sizeof(pthread_t))))
        TEST_ERROR;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Set the initial state of the shared value (0) */
    OPA_store_int(&shared_val, 0);

    /* Create the threads.  All the unique values must add up to 0. */
    for(i=0; i<nthreads; i++) {
        if(i & 1) {
            if(pthread_create(&threads[i], &ptattr, threaded_fad_helper,
                    &shared_val)) TEST_ERROR;
        } else
            if(pthread_create(&threads[i], &ptattr, threaded_fai_helper,
                    &shared_val)) TEST_ERROR;
    } /* end for */

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<nthreads; i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != INCR_DECR_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), INCR_DECR_EXPECTED));

    /* Free memory */
    free(threads);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("fetch and incr/decr", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_fai_fad() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_fai_ret_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_fai_ret
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Friday, June 5, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_fai_ret_helper(void *_udata)
{
    fai_ret_t           *udata = (fai_ret_t *)_udata;
    int                 ret, prev = INT_MIN;
    unsigned            niter = FAI_RET_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++) {
        /* Add -1 to the shared value */
        ret = OPA_fetch_and_incr_int(udata->shared_val);

        /* Verify that the value returned is greater than the previous return */
        if(ret <= prev) {
            printf("\n    Unexpected return: %d is not greater than %d  ", ret, prev);
            (udata->nerrors)++;
        } /* end if */

        /* Check if the return value is -1 */
        if(ret == -1)
            (udata->nm1)++;

        /* update prev */
        prev = ret;
    } /* end for */

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_fai_ret_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_fai_ret
 *
 * Purpose: Tests atomicity of OPA_fetch_and_incr_int.  Launches nthreads
 *          threads, each of which repeatedly adds -1 to a shared
 *          variable.  Verifies that the value returned is always
 *          decreasing, and that it returns 1 exactly once.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Friday, June 5, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_fai_ret(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    fai_ret_t           *thread_data = NULL; /* User data structs for threads */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    int                 nerrors = 0;    /* Number of errors */
    int                 n1 = 0;         /* # of times fai returned 1 */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("fetch and incr return values", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (fai_ret_t *) calloc(nthreads, sizeof(fai_ret_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, -FAI_RET_NITER);
    for(i=0; i<nthreads; i++)
        thread_data[i].shared_val = &shared_val;
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_fai_ret_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_fai_ret_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Count number of errors and number of times 1 was returned */
    for(i=0; i<nthreads; i++) {
        nerrors += thread_data[i].nerrors;
        n1 += thread_data[i].nm1;
    } /* end for */

    /* Verify that no errors were reported */
    if(nerrors)
        FAIL_OP_ERROR(printf("    %d unexpected return%s from OPA_fetch_and_incr_int\n",
                nerrors, nerrors == 1 ? "" : "s"));

    /* Verify that OPA_fetch_and_add_int returned 1 expactly once */
    if(n1 != 1)
        FAIL_OP_ERROR(printf("    OPA_fetch_and_add_int returned -1 %d times.  Expected: 1\n",
                n1));

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != FAI_RET_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), FAI_RET_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("fetch and incr return values", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_fai_ret() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_fad_ret_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_fad_ret
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Friday, June 5, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_fad_ret_helper(void *_udata)
{
    faa_ret_t           *udata = (faa_ret_t *)_udata;
    int                 ret, prev = INT_MAX;
    unsigned            niter = FAA_RET_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++) {
        /* Add -1 to the shared value */
        ret = OPA_fetch_and_decr_int(udata->shared_val);

        /* Verify that the value returned is less than the previous return */
        if(ret >= prev) {
            printf("\n    Unexpected return: %d is not less than %d  ", ret, prev);
            (udata->nerrors)++;
        } /* end if */

        /* Check if the return value is 1 */
        if(ret == 1)
            (udata->n1)++;

        /* update prev */
        prev = ret;
    } /* end for */

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_fad_ret_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_fad_ret
 *
 * Purpose: Tests atomicity of OPA_fetch_and_decr_int.  Launches nthreads
 *          threads, each of which repeatedly adds -1 to a shared
 *          variable.  Verifies that the value returned is always
 *          decreasing, and that it returns 1 exactly once.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Friday, June 5, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_fad_ret(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    faa_ret_t           *thread_data = NULL; /* User data structs for threads */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    int                 nerrors = 0;    /* Number of errors */
    int                 n1 = 0;         /* # of times faa returned 1 */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("fetch and decr return values", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (faa_ret_t *) calloc(nthreads, sizeof(faa_ret_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, FAA_RET_NITER);
    for(i=0; i<nthreads; i++)
        thread_data[i].shared_val = &shared_val;
    thread_data[nthreads-1].master_thread = 1;


    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_fad_ret_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_fad_ret_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Count number of errors and number of times 1 was returned */
    for(i=0; i<nthreads; i++) {
        nerrors += thread_data[i].nerrors;
        n1 += thread_data[i].n1;
    } /* end for */

    /* Verify that no errors were reported */
    if(nerrors)
        FAIL_OP_ERROR(printf("    %d unexpected return%s from OPA_fetch_and_decr_int\n",
                nerrors, nerrors == 1 ? "" : "s"));

    /* Verify that OPA_fetch_and_add_int returned 1 expactly once */
    if(n1 != 1)
        FAIL_OP_ERROR(printf("    OPA_fetch_and_add_int returned 1 %d times.  Expected: 1\n",
                n1));

    /* Verify that the shared value contains the expected result (0) */
    if(OPA_load_int(&shared_val) != FAA_RET_EXPECTED)
        FAIL_OP_ERROR(printf("    Unexpected result: %d expected: %d\n",
                OPA_load_int(&shared_val), FAA_RET_EXPECTED));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("fetch and decr return values", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_fad_ret() */


/*-------------------------------------------------------------------------
 * Function: test_simple_cas_int
 *
 * Purpose: Tests basic functionality of OPA_cas_int with a single thread.
 *          Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, June 9, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_cas_int(void)
{
    OPA_int_t   a;

    TESTING("simple integer compare-and-swap functionality", 0);

    /* Store 0 in a */
    OPA_store_int(&a, 0);

    /* Compare and swap multiple times, verify return value and final result */
    if(0 != OPA_cas_int(&a, 1, INT_MAX)) TEST_ERROR;
    if(0 != OPA_cas_int(&a, 0, INT_MAX)) TEST_ERROR;
    if(INT_MAX != OPA_cas_int(&a, INT_MAX, INT_MIN)) TEST_ERROR;
    if(INT_MIN != OPA_cas_int(&a, INT_MAX, 1)) TEST_ERROR;
    if(INT_MIN != OPA_cas_int(&a, INT_MIN, 1)) TEST_ERROR;
    if(1 != OPA_load_int(&a)) TEST_ERROR;

    PASSED();
    return 0;

error:
    return 1;
} /* end test_simple_cas_int() */


/*-------------------------------------------------------------------------
 * Function: test_simple_cas_ptr
 *
 * Purpose: Tests basic functionality of OPA_cas_ptr with a single thread.
 *          Does not test atomicity of operations.
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, June 9, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_simple_cas_ptr(void)
{
    OPA_ptr_t   a;
    void        *ptr1 = malloc(1);      /* Pointers to assign to a */
    void        *ptr2 = malloc(1);
    void        *ptr3 = malloc(1);
    void        *ptr4 = malloc(1);

    TESTING("simple pointer compare-and-swap functionality", 0);

    /* Store 0 in a */
    OPA_store_ptr(&a, ptr1);

    /* Compare and swap multiple times, verify return value and final result */
    if(ptr1 != OPA_cas_ptr(&a, ptr2, ptr3)) TEST_ERROR;
    if(ptr1 != OPA_cas_ptr(&a, ptr1, ptr3)) TEST_ERROR;
    if(ptr3 != OPA_cas_ptr(&a, ptr3, ptr4)) TEST_ERROR;
    if(ptr4 != OPA_cas_ptr(&a, ptr3, ptr2)) TEST_ERROR;
    if(ptr4 != OPA_cas_ptr(&a, ptr4, ptr2)) TEST_ERROR;
    if(ptr2 != OPA_load_ptr(&a)) TEST_ERROR;
    
    if(ptr1) free(ptr1);
    if(ptr2) free(ptr2);
    if(ptr3) free(ptr3);
    if(ptr4) free(ptr4);

    PASSED();
    return 0;

error:
    if(ptr1) free(ptr1);
    if(ptr2) free(ptr2);
    if(ptr3) free(ptr3);
    if(ptr4) free(ptr4);

    return 1;
} /* end test_simple_cas_ptr() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_cas_int_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_cas_int
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Tuesday, June 9, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_cas_int_helper(void *_udata)
{
    cas_int_t           *udata = (cas_int_t *)_udata;
    int                 thread_id = udata->threadno;
    int                 next_id = (thread_id + 1) % num_threads[curr_test];
    unsigned            niter = CAS_INT_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        if(OPA_cas_int(udata->shared_val, thread_id, next_id) == thread_id)
            udata->nsuccess++;

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_cas_int_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_cas_int
 *
 * Purpose: Tests atomicity of OPA_cas_int.  Launches nthreads threads,
 *          each of which continually tries to compare-and-swap a shared
 *          value with its thread id (oldv) and thread id + 1 % n (newv).
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, June 9, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_cas_int(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    cas_int_t           *thread_data = NULL; /* User data structs for threads */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("integer compare-and-swap", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (cas_int_t *) calloc(nthreads, sizeof(cas_int_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, 0);
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].threadno = i;
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_cas_int_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_cas_int_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that cas succeeded at least once */
    if(thread_data[0].nsuccess == 0)
        FAIL_OP_ERROR(printf("    Compare-and-swap never succeeded\n"));

    /* Verify that the number of successes does not increase with increasing
     * thread number, and also that the shared value's final value is consistent
     * with the numbers of successes */
    if(nthreads > 1)
        for(i=1; i<nthreads; i++) {
            if(thread_data[i].nsuccess > thread_data[i-1].nsuccess)
                FAIL_OP_ERROR(printf("    Thread %d succeeded more times than thread %d\n",
                        i, i-1));

            if((thread_data[i].nsuccess < thread_data[i-1].nsuccess)
                    && (OPA_load_int(&shared_val) != i))
                FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));
        } /* end for */
    if((thread_data[0].nsuccess == thread_data[nthreads-1].nsuccess)
            && (OPA_load_int(&shared_val) != 0))
        FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));

    /* Verify that the number of successes for the first threads is equal to or
     * one greater than the number of successes for the last thread.  Have
     * already determined that it is not less in the previous test. */
    if(thread_data[0].nsuccess > (thread_data[nthreads-1].nsuccess + 1))
        FAIL_OP_ERROR(printf("    Thread 0 succeeded %d times more than thread %d\n",
                thread_data[0].nsuccess - thread_data[nthreads-1].nsuccess,
                nthreads - 1));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("integer compare-and-swap", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_cas_int() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: threaded_cas_ptr_helper
 *
 * Purpose: Helper (thread) routine for test_threaded_cas_ptr
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Monday, August 3, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *threaded_cas_ptr_helper(void *_udata)
{
    cas_ptr_t           *udata = (cas_ptr_t *)_udata;
    int                 *thread_id = udata->threadno;
    int                 *next_id = thread_id + 1;
    unsigned            niter = CAS_PTR_NITER;
    unsigned            i;

    /* This is the equivalent of the "modulus" operation, but for pointers */
    if(next_id > udata->max_threadno)
        next_id = (int *) 0;

    /* Main loop */
    for(i=0; i<niter; i++)
        if(OPA_cas_ptr(udata->shared_val, (void *) thread_id, (void *) next_id) == (void *) thread_id)
            udata->nsuccess++;

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end threaded_cas_ptr_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_threaded_cas_ptr
 *
 * Purpose: Tests atomicity of OPA_cas_ptr.  Launches nthreads threads,
 *          each of which continually tries to compare-and-swap a shared
 *          value with its thread id (oldv) and thread id + 1 % n (newv).
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Monday, August 3, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_threaded_cas_ptr(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    cas_ptr_t           *thread_data = NULL; /* User data structs for threads */
    OPA_ptr_t           shared_val;     /* Integer shared between threads */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("pointer compare-and-swap", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data = (cas_ptr_t *) calloc(nthreads, sizeof(cas_ptr_t))))
        TEST_ERROR;

    /* Initialize thread data structs */
    OPA_store_ptr(&shared_val, (void *) 0);
    thread_data[0].shared_val = &shared_val;
    thread_data[0].threadno = (int *) 0;
    for(i=1; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].threadno = (int *) 0 + i;
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;
    for(i=0; i<nthreads; i++)
        thread_data[i].max_threadno = thread_data[nthreads-1].threadno;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, threaded_cas_ptr_helper,
                &thread_data[i])) TEST_ERROR;
    (void)threaded_cas_ptr_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Verify that cas succeeded at least once */
    if(thread_data[0].nsuccess == 0)
        FAIL_OP_ERROR(printf("    Compare-and-swap never succeeded\n"));

    /* Verify that the number of successes does not increase with increasing
     * thread number, and also that the shared value's final value is consistent
     * with the numbers of successes */
    if(nthreads > 1)
        for(i=1; i<nthreads; i++) {
            if(thread_data[i].nsuccess > thread_data[i-1].nsuccess)
                FAIL_OP_ERROR(printf("    Thread %d succeeded more times than thread %d\n",
                        i, i-1));

            if((thread_data[i].nsuccess < thread_data[i-1].nsuccess)
                    && (OPA_load_ptr(&shared_val) != thread_data[i].threadno))
                FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));
        } /* end for */
    if((thread_data[0].nsuccess == thread_data[nthreads-1].nsuccess)
            && (OPA_load_ptr(&shared_val) != (void *) 0))
        FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));

    /* Verify that the number of successes for the first threads is equal to or
     * one greater than the number of successes for the last thread.  Have
     * already determined that it is not less in the previous test. */
    if(thread_data[0].nsuccess > (thread_data[nthreads-1].nsuccess + 1))
        FAIL_OP_ERROR(printf("    Thread 0 succeeded %d times more than thread %d\n",
                thread_data[0].nsuccess - thread_data[nthreads-1].nsuccess,
                nthreads - 1));

    /* Free memory */
    free(threads);
    free(thread_data);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("pointer compare-and-swap", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_threaded_cas_ptr() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: grouped_cas_int_helper
 *
 * Purpose: Helper (thread) routine for test_grouped_cas_int
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Tuesday, August 4, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *grouped_cas_int_helper(void *_udata)
{
    grouped_cas_int_t   *udata = (grouped_cas_int_t *)_udata;
    int                 group_id = udata->groupno;
    int                 next_id = (group_id + 1) % udata->ngroups;
    unsigned            niter = GROUPED_CAS_INT_NITER;
    unsigned            i;

    /* Main loop */
    for(i=0; i<niter; i++)
        if(OPA_cas_int(udata->shared_val, group_id, next_id) == group_id)
            udata->nsuccess++;

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end grouped_cas_int_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_grouped_cas_int
 *
 * Purpose: Tests atomicity of OPA_cas_int.  Launches nthreads threads,
 *          subdivided into nthreads/4 or 2 groups, whichever is greater.
 *          Each thread continually tries to compare-and-swap a shared
 *          value with its group id (oldv) and group id + 1 % ngroups
 *          (newv).
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Tuesday, August 4, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_grouped_cas_int(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    grouped_cas_int_t   *thread_data = NULL; /* User data structs for threads */
    OPA_int_t           shared_val;     /* Integer shared between threads */
    int                 ngroups;        /* Number of groups of threads */
    int                 threads_per_group; /* Threads per group */
    int                 *group_success = NULL; /* Number of successes for each group */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("grouped integer compare-and-swap", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data =
            (grouped_cas_int_t *) calloc(nthreads, sizeof(grouped_cas_int_t))))
        TEST_ERROR;

    /* Calculate number of groups and threads per group */
    if((nthreads / GROUPED_CAS_INT_TPG) >= 2)
        threads_per_group = GROUPED_CAS_INT_TPG;
    else
        threads_per_group = (nthreads + 1) / 2;
    ngroups = (nthreads + threads_per_group - 1) / threads_per_group;

    /* Initialize thread data structs */
    OPA_store_int(&shared_val, 0);
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].groupno = i / threads_per_group;
        thread_data[i].ngroups = ngroups;
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, grouped_cas_int_helper,
                &thread_data[i])) TEST_ERROR;
    (void)grouped_cas_int_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Calculate the total number of successes for each group */
    if(NULL == (group_success = (int *) calloc(ngroups, sizeof(int))))
        TEST_ERROR;
    for(i=0; i<nthreads; i++)
        group_success[thread_data[i].groupno] += thread_data[i].nsuccess;

    /* Verify that cas succeeded at least once */
    if(group_success[0] == 0)
        FAIL_OP_ERROR(printf("    Compare-and-swap never succeeded\n"));

    /* Verify that the number of successes does not increase with increasing
     * group number, and also that the shared value's final value is consistent
     * with the numbers of successes */
    if(nthreads > 1)
        for(i=1; i<ngroups; i++) {
            if(group_success[i] > group_success[i-1])
                FAIL_OP_ERROR(printf("    Group %d succeeded more times than group %d\n",
                        i, i-1));

            if((group_success[i] < group_success[i-1])
                    && (OPA_load_int(&shared_val) != i))
                FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));
        } /* end for */
    if((group_success[0] == group_success[ngroups-1])
            && (OPA_load_int(&shared_val) != 0))
        FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));

    /* Verify that the number of successes for the first threads is equal to or
     * one greater than the number of successes for the last thread.  Have
     * already determined that it is not less in the previous test. */
    if(group_success[0] > (group_success[ngroups-1] + 1))
        FAIL_OP_ERROR(printf("    Group 0 succeeded %d times more than group %d\n",
                thread_data[0].nsuccess - thread_data[nthreads-1].nsuccess,
                nthreads - 1));

    /* Free memory */
    free(threads);
    free(thread_data);
    free(group_success);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("grouped integer compare-and-swap", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    if(group_success) free(group_success);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_grouped_cas_int() */


#if defined(OPA_HAVE_PTHREAD_H)
/*-------------------------------------------------------------------------
 * Function: grouped_cas_ptr_helper
 *
 * Purpose: Helper (thread) routine for test_grouped_cas_ptr
 *
 * Return: NULL
 *
 * Programmer: Neil Fortner
 *             Monday, August 10, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static void *grouped_cas_ptr_helper(void *_udata)
{
    grouped_cas_ptr_t   *udata = (grouped_cas_ptr_t *)_udata;
    int                 *group_id = udata->groupno;
    int                 *next_id = group_id + 1;
    unsigned            niter = GROUPED_CAS_PTR_NITER;
    unsigned            i;

    /* This is the equivalent of the "modulus" operation, but for pointers */
    if(next_id > udata->max_groupno)
        next_id = (int *) 0;

    /* Main loop */
    for(i=0; i<niter; i++)
        if(OPA_cas_ptr(udata->shared_val, (void *) group_id, (void *) next_id) == (void *) group_id)
            udata->nsuccess++;

    /* Exit */
    if(udata->master_thread)
        return(NULL);
    else
        pthread_exit(NULL);
} /* end grouped_cas_ptr_helper() */
#endif /* OPA_HAVE_PTHREAD_H */


/*-------------------------------------------------------------------------
 * Function: test_grouped_cas_ptr
 *
 * Purpose: Tests atomicity of OPA_cas_ptr.  Launches nthreads threads,
 *          subdivided into nthreads/4 or 2 groups, whichever is greater.
 *          Each thread continually tries to compare-and-swap a shared
 *          value with its group id (oldv) and group id + 1 % ngroups
 *          (newv).
 *
 * Return: Success: 0
 *         Failure: 1
 *
 * Programmer: Neil Fortner
 *             Monday, August 10, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static int test_grouped_cas_ptr(void)
{
#if defined(OPA_HAVE_PTHREAD_H)
    pthread_t           *threads = NULL; /* Threads */
    pthread_attr_t      ptattr;         /* Thread attributes */
    grouped_cas_ptr_t   *thread_data = NULL; /* User data structs for threads */
    OPA_ptr_t           shared_val;     /* Integer shared between threads */
    int                 ngroups;        /* Number of groups of threads */
    int                 threads_per_group; /* Threads per group */
    int                 *group_success = NULL; /* Number of successes for each group */
    unsigned            nthreads = num_threads[curr_test];
    unsigned            i;

    TESTING("grouped pointer compare-and-swap", nthreads);

    /* Allocate array of threads */
    if(NULL == (threads = (pthread_t *) malloc((nthreads - 1) * sizeof(pthread_t))))
        TEST_ERROR;

    /* Allocate array of thread data */
    if(NULL == (thread_data =
            (grouped_cas_ptr_t *) calloc(nthreads, sizeof(grouped_cas_ptr_t))))
        TEST_ERROR;

    /* Calculate number of groups and threads per group */
    if((nthreads / GROUPED_CAS_PTR_TPG) >= 2)
        threads_per_group = GROUPED_CAS_PTR_TPG;
    else
        threads_per_group = (nthreads + 1) / 2;
    ngroups = (nthreads + threads_per_group - 1) / threads_per_group;

    /* Initialize thread data structs */
    OPA_store_ptr(&shared_val, (void *) 0);
    for(i=0; i<nthreads; i++) {
        thread_data[i].shared_val = &shared_val;
        thread_data[i].groupno = (int *) 0 + (i / threads_per_group);
    } /* end for */
    thread_data[nthreads-1].master_thread = 1;
    for(i=0; i<nthreads; i++)
        thread_data[i].max_groupno = thread_data[nthreads-1].groupno;

    /* Set threads to be joinable */
    pthread_attr_init(&ptattr);
    pthread_attr_setdetachstate(&ptattr, PTHREAD_CREATE_JOINABLE);

    /* Create the threads */
    for(i=0; i<(nthreads - 1); i++)
        if(pthread_create(&threads[i], &ptattr, grouped_cas_ptr_helper,
                &thread_data[i])) TEST_ERROR;
    (void)grouped_cas_ptr_helper(&thread_data[i]);

    /* Free the attribute */
    if(pthread_attr_destroy(&ptattr)) TEST_ERROR;

    /* Join the threads */
    for (i=0; i<(nthreads - 1); i++)
        if(pthread_join(threads[i], NULL)) TEST_ERROR;

    /* Calculate the total number of successes for each group */
    if(NULL == (group_success = (int *) calloc(ngroups, sizeof(int))))
        TEST_ERROR;
    for(i=0; i<nthreads; i++)
        group_success[(int) (thread_data[i].groupno - (int *) 0)] +=
                thread_data[i].nsuccess;

    /* Verify that cas succeeded at least once */
    if(group_success[0] == 0)
        FAIL_OP_ERROR(printf("    Compare-and-swap never succeeded\n"));

    /* Verify that the number of successes does not increase with increasing
     * group number, and also that the shared value's final value is consistent
     * with the numbers of successes */
    if(nthreads > 1)
        for(i=1; i<ngroups; i++) {
            if(group_success[i] > group_success[i-1])
                FAIL_OP_ERROR(printf("    Group %d succeeded more times than group %d\n",
                        i, i-1));

            if((group_success[i] < group_success[i-1])
                    && (OPA_load_ptr(&shared_val) != ((int *) 0) + i))
                FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));
        } /* end for */
    if((group_success[0] == group_success[ngroups-1])
            && (OPA_load_ptr(&shared_val) != (int *) 0))
        FAIL_OP_ERROR(printf("    Number of successes is inconsistent\n"));

    /* Verify that the number of successes for the first threads is equal to or
     * one greater than the number of successes for the last thread.  Have
     * already determined that it is not less in the previous test. */
    if(group_success[0] > (group_success[ngroups-1] + 1))
        FAIL_OP_ERROR(printf("    Group 0 succeeded %d times more than group %d\n",
                thread_data[0].nsuccess - thread_data[nthreads-1].nsuccess,
                nthreads - 1));

    /* Free memory */
    free(threads);
    free(thread_data);
    free(group_success);

    PASSED();

#else /* OPA_HAVE_PTHREAD_H */
    TESTING("grouped pointer compare-and-swap", 0);
    SKIPPED();
    puts("    pthread.h not available");
#endif /* OPA_HAVE_PTHREAD_H */

    return 0;

#if defined(OPA_HAVE_PTHREAD_H)
error:
    if(threads) free(threads);
    if(thread_data) free(thread_data);
    if(group_success) free(group_success);
    return 1;
#endif /* OPA_HAVE_PTHREAD_H */
} /* end test_grouped_cas_ptr() */


/*-------------------------------------------------------------------------
 * Function:    main
 *
 * Purpose:     Tests the opa primitives
 *
 * Return:      Success:        exit(0)
 *
 *              Failure:        exit(1)
 *
 * Programmer:  Neil Fortner
 *              Thursday, March 19, 2009
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
int main(int argc, char **argv)
{
    unsigned nerrors = 0;
#if defined(OPA_USE_LOCK_BASED_PRIMITIVES)
    pthread_mutex_t shm_lock;
    OPA_Interprocess_lock_init(&shm_lock, 1/*isLeader*/);
#endif

    /* Simple tests */
    nerrors += test_simple_loadstore_int();
    nerrors += test_simple_loadstore_ptr();
    nerrors += test_simple_add_incr_decr();
    nerrors += test_simple_decr_and_test();
    nerrors += test_simple_faa_fai_fad();
    nerrors += test_simple_cas_int();
    nerrors += test_simple_cas_ptr();

    /* Loop over test configurations */
    for(curr_test=0; curr_test<num_thread_tests; curr_test++) {
        /* Threaded tests */
        nerrors += test_threaded_loadstore_int();
        nerrors += test_threaded_loadstore_ptr();
        nerrors += test_threaded_add();
        nerrors += test_threaded_incr_decr();
        nerrors += test_threaded_decr_and_test();
        nerrors += test_threaded_faa();
        nerrors += test_threaded_faa_ret();
        nerrors += test_threaded_fai_fad();
        nerrors += test_threaded_fai_ret();
        nerrors += test_threaded_fad_ret();
        nerrors += test_threaded_cas_int();
        nerrors += test_threaded_cas_ptr();
        nerrors += test_grouped_cas_int();
        nerrors += test_grouped_cas_ptr();
    } /* end for */

    if(nerrors)
        goto error;
    printf("All primitives tests passed.\n");

    return 0;

error:
    if(!nerrors)
        nerrors = 1;
    printf("***** %d PRIMITIVES TEST%s FAILED! *****\n",
            nerrors, 1 == nerrors ? "" : "S");
    return 1;
} /* end main() */

