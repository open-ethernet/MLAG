/* Copyright (c) 2014  Mellanox Technologies, Ltd. All rights reserved.
 *
 * This software is available to you under BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <complib/sx_rpc.h>
#include <complib/cl_init.h>
#include <complib/cl_timer.h>
#include <utils/mlag_log.h>
#include <utils/mlag_bail.h>
#include <mlag_common/mlag_common.h>
#include <utils/mlag_events.h>
#include "mlag_init.h"
#include "mlag_main.h"
#include "mlag_internal_api.h"

/************************************************
 *  Local Defines
 ***********************************************/

#undef  __MODULE__
#define __MODULE__ MLAG

#define MLAG_SIGNAL_NUM 5


/************************************************
 *  Local Macros
 ***********************************************/


/************************************************
 *  Local Type definitions
 ***********************************************/


/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static struct mlag_args_t mlag_args;

static struct option mlag_long_options[] = {
    {"debug_lib",   required_argument,      NULL,   MLAG_DEBUG      },
    {"logger",      required_argument,      NULL,   MLAG_LOGGER     },
    {"connector",   required_argument,      NULL,   MLAG_CONNECTOR  },
    {"help",        no_argument,            NULL,   'h'             },
    {"verbose",     required_argument,      NULL,   'v'             },
    {"version",     no_argument,            NULL,   MLAG_VERSION    },
    {0,             0,                      0,      0               }
};

static int mlag_signals[MLAG_SIGNAL_NUM] =
{SIGINT, SIGTERM, SIGQUIT, SIGABRT, SIGHUP};

static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = MLAG_VERBOSITY_LEVEL_NOTICE;
static void *debug_dll = NULL;
static void *mlag_connector_dll = NULL;
static void *logger_dll = NULL;

/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/

/*
 *  This function prints usage message
 *
 * @return void
 */
static void
show_help()
{
    printf( "\tmLAG Process.\n"
            "\t=============================\n"
            "\tUsage: mlag [options]\n\n"
            "\tOptions:\n"
            "\t(-v<level>|--verbose=<level>|)	Set Verbosity (dependent) level (default = 3):\n"
            "\t                                 0 = None.\n"
            "\t                                 1 = Error messages only.\n"
            "\t                                 2 = Error & Warning messages.\n"
            "\t                                 3 = Error, Warning & Info messages.\n"
            "\t                                 4 = Error, Warning, Info & Verbose messages.\n"
            "\t                                 5 = Error, Warning, Info, Verbose & Debug messages.\n"
            "\t--debug_lib                      Path to dynamically loaded debug library.\n"
            "\t--logger                         Path to dynamically loaded logging callback.\n"
            "\t--connector                      Path to dynamically loaded connector library.\n"
            "\t--version                        Report version & exit normally.\n"
            "\t(-h|--help)                      Show this help message & exit normally.\n");
    exit(0);
}

/*
 *  This function prints error message
 *
 * @return void
 */
static void
show_error()
{
    fprintf(stderr,
            "Bad parameter(s). Use --help to get parameters summary\n");
    exit(1);
}

/*
 *  This function is called for handling signals that were
 *  registered
 *
 * @param[in] signum - signal number
 *
 * @return 0 if operation completes successfully.
 */
static void
signal_handler(int signum)
{
    pthread_t t_id;
    int err;
    t_id = pthread_self();

    MLAG_LOG(MLAG_LOG_NOTICE, "Signal %u caught using thread %u context\n",
             (unsigned int)signum, (unsigned int)t_id);

    err = mlag_deinit();
    MLAG_BAIL_ERROR(err);

bail:
    return;
}

/*
 *  This function registers signal handler to defined signals
 *
 * @return 0 if operation completes successfully.
 */
static int
catch_signals()
{
    struct sigaction action;
    int i = 0, ret = 0, err = 0;

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    for (i = 0; i < MLAG_SIGNAL_NUM; ++i) {
        ret = sigaction(mlag_signals[i], &action, NULL);
        if (ret == -1) {
            MLAG_LOG(MLAG_LOG_ERROR, "sigaction error: %u\n", ret);
            err = -EIO;
            goto bail;
        }
    }

    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    ret = sigaction(SIGPIPE, &action, NULL);
    if (ret == -1) {
        MLAG_LOG(MLAG_LOG_ERROR, "sigaction error: %u\n", ret);
        err = -EIO;
        goto bail;
    }

bail:
    return err;
}

/*
 *  This function is called for handling signals that were
 *  registered
 *
 * @param[in] signum - signal number
 *
 * @return 0 if operation completes successfully.
 */
static int
parse_args(int argc, char **argv)
{
    int verbosity = MLAG_VERBOSITY_LEVEL_NOTICE;
    int ret, c, verbose_flag = 0;
    char *errmsg = NULL;
    int err = 0;
    int option_index = 0;

    /* default verbosity */
    mlag_args.verbosity_level = MLAG_VERBOSITY_LEVEL_NOTICE;

    while (TRUE) {
        c = getopt_long(argc, argv, "hv:", mlag_long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) {
            break;
        }

        switch (c) {
        case MLAG_DEBUG:
            /* Allow to attach a debug library to the process */
            debug_dll = dlopen(optarg, RTLD_LAZY);
            errmsg = dlerror();
            if (errmsg) {
                fprintf(stderr, "Could not load debug library: %s.\n", errmsg);
                exit(1);
            }
            *(void **)(&(mlag_args.debug_cb)) = dlsym(debug_dll,
                                                      "mlag_debug_init");
            errmsg = dlerror();
            if (errmsg) {
                dlclose(debug_dll);
                *(void **)(&(mlag_args.debug_cb)) = NULL;
                fprintf(stderr, "Could not find symbol mlag_debug_init: %s.\n",
                        errmsg);
                exit(1);
            }
            *(void **) (&(mlag_args.debug_deinit_cb)) = dlsym(
                debug_dll, "mlag_debug_deinit");
            errmsg = dlerror();
            if (errmsg) {
                dlclose(debug_dll);
                *(void **) (&(mlag_args.debug_deinit_cb)) = NULL;
                fprintf(stderr,
                        "Could not find symbol mlag_debug_deinit: %s.\n",
                        errmsg);
                exit(1);
            }
            break;
        case MLAG_CONNECTOR:
            mlag_connector_dll = dlopen(optarg, RTLD_LAZY);
            errmsg = dlerror();
            if (errmsg) {
                fprintf(stderr, "Could not load mLAG connector library: %s.\n",
                        errmsg);
                exit(1);
            }
            *(void **) (&(mlag_args.connector_cb)) = dlsym(
                mlag_connector_dll, "mlagd_init");
            errmsg = dlerror();
            if (errmsg) {
                dlclose(mlag_connector_dll);
                *(void **) (&(mlag_args.connector_cb)) = NULL;
                fprintf(stderr, "Could not find symbol mlagd_init: %s.\n",
                        errmsg);
                exit(1);
            }

            *(void **) (&(mlag_args.connector_deinit_cb)) = dlsym(
                mlag_connector_dll, "mlagd_deinit");
            errmsg = dlerror();
            if (errmsg) {
                dlclose(mlag_connector_dll);
                *(void **) (&(mlag_args.connector_deinit_cb)) = NULL;
                fprintf(stderr, "Could not find symbol mlagd_deinit: %s.\n",
                        errmsg);
                exit(1);
            }
            break;
        case MLAG_LOGGER:
            logger_dll = dlopen(optarg, RTLD_LAZY);
            errmsg = dlerror();
            if (errmsg) {
                fprintf(stderr, "Could not load mLAG logger library: %s.\n",
                        errmsg);
                exit(1);
            }
            *(void **) (&(mlag_args.log_cb)) = dlsym(
                logger_dll, "mlagd_log_cb");
            errmsg = dlerror();
            if (errmsg) {
                dlclose(logger_dll);
                *(void **) (&(mlag_args.log_cb)) = NULL;
                fprintf(stderr, "Could not find symbol mlagd_log_cb: %s.\n",
                        errmsg);
                exit(1);
            }
            break;
        case MLAG_VERSION:
            printf("1.0\n");
            exit(0);
        case 'v':
            verbose_flag++;
            ret = sscanf(optarg, "%u", &verbosity);
            if (ret != 1) {
                show_error();
            }
            mlag_args.verbosity_level = verbosity;
            break;
        case 'h':
            show_help();
            break;
        case '?':
        default:
            /* getopt_long already printed an error message. */
            show_error();
            break;
        }
    }

    /* Print any remaining command line arguments (not options). */
    if (optind < argc) {
        fprintf(stderr, "Non-option ARGV-elements: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        show_error();
    }

    return err;
}

/*
 *  main function
 *
 * @param[in] argc - arguments count
 * @param[in] argv - arguments value
 *
 * @return 0 if operation completes successfully.
 */
int
main(int argc, char **argv)
{
    int err = 0;

    err = parse_args(argc, argv);
    MLAG_BAIL_ERROR(err)

    err = MLAG_LOG_INIT(mlag_args.log_cb);

    MLAG_LOG(MLAG_LOG_NOTICE, "MLAG Starting \n");

    err = mlag_rpc_init_procedure(mlag_args.log_cb);
    if (err) {
        MLAG_LOG(MLAG_LOG_ERROR, "RPC procedure failed\n");
        goto bail;
    }

    /* Call connector callback before main loop */
    if (mlag_args.connector_cb != NULL) {
        mlag_args.connector_cb(mlag_deinit);
    }
    else {
        /* Initialize signals handling */
        err = catch_signals();
        MLAG_BAIL_ERROR_MSG(err, "Failed to register signal handler\n");
    }

    /* Initiate debug library if needed */
    if (mlag_args.debug_cb != NULL) {
        mlag_args.debug_cb();
    }

    err = mlag_init(mlag_args.log_cb);
    MLAG_BAIL_ERROR_MSG(err, "Failed to initialize mlag\n");

    /* Receive actions and send return value to/from clients */
    err = mlag_rpc_start_loop();
    MLAG_BAIL_ERROR(err);

bail:

    if (mlag_args.connector_cb != NULL) {
        mlag_args.connector_deinit_cb();
    }

    if (mlag_args.debug_cb != NULL) {
        mlag_args.debug_deinit_cb();
    }

    MLAG_LOG_CLOSE();

    if (mlag_connector_dll) {
        dlclose(mlag_connector_dll);
    }

    if (debug_dll) {
        dlclose(debug_dll);
    }

    return err;
}

/**
 * Clean the mlag main resources
 */
void
mlag_main_deinit()
{
    if (mlag_args.debug_cb != NULL) {
        mlag_args.debug_deinit_cb();
    }

    MLAG_LOG_CLOSE();

    if (mlag_connector_dll) {
        dlclose(mlag_connector_dll);
    }

    if (debug_dll) {
        dlclose(debug_dll);
    }

    return;
}
