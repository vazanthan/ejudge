/* -*- c -*- */
/* $Id$ */

/* Copyright (C) 2000-2005 Alexander Chernov <cher@ispras.ru> */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "config.h"

#include "prepare.h"
#include "settings.h"
#include "varsubst.h"
#include "version.h"

#include "fileutl.h"
#include "sformat.h"
#include "teamdb.h"
#include "prepare_serve.h"

#include <reuse/xalloc.h>
#include <reuse/logger.h>
#include <reuse/osdeps.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

struct generic_section_config *config;
struct section_global_data    *global;

struct section_language_data *langs[MAX_LANGUAGE + 1];
struct section_problem_data  *probs[MAX_PROBLEM + 1];
struct section_tester_data   *testers[MAX_TESTER + 1];

int max_lang;
int max_prob;
int max_tester;

/* new userlist-server interaction */
struct contest_desc *cur_contest;

static struct section_problem_data  *abstr_probs[MAX_PROBLEM + 1];
static struct section_tester_data   *abstr_testers[MAX_TESTER + 1];

static int max_abstr_prob;
static int max_abstr_tester;

#define XFSIZE(t, x) (sizeof(((t*) 0)->x))

#define GLOBAL_OFFSET(x)   XOFFSET(struct section_global_data, x)
#define GLOBAL_SIZE(x)     XFSIZE(struct section_global_data, x)
#define GLOBAL_PARAM(x, t) { #x, t, GLOBAL_OFFSET(x), GLOBAL_SIZE(x) }
#define GLOBAL_ALIAS(a, x, t) { #a, t, GLOBAL_OFFSET(x), GLOBAL_SIZE(x) }
static struct config_parse_info section_global_params[] =
{
  GLOBAL_PARAM(name, "s"),
  GLOBAL_PARAM(sleep_time, "d"),
  GLOBAL_PARAM(serve_sleep_time, "d"),
  GLOBAL_PARAM(contest_time, "d"),
  GLOBAL_PARAM(max_run_size, "d"),
  GLOBAL_PARAM(max_run_total, "d"),
  GLOBAL_PARAM(max_run_num, "d"),
  GLOBAL_PARAM(max_clar_size, "d"),
  GLOBAL_PARAM(max_clar_total, "d"),
  GLOBAL_PARAM(max_clar_num, "d"),

  GLOBAL_PARAM(board_fog_time, "d"),
  GLOBAL_PARAM(board_unfog_time, "d"),

  // aliases for the existing configuration variables
  GLOBAL_ALIAS(stand_freeze_time, board_fog_time, "d"),
  GLOBAL_ALIAS(stand_melt_time, board_unfog_time, "d"),

  GLOBAL_PARAM(autoupdate_standings, "d"),
  GLOBAL_PARAM(team_enable_src_view, "d"),
  GLOBAL_PARAM(team_enable_rep_view, "d"),
  GLOBAL_PARAM(team_enable_ce_view, "d"),
  GLOBAL_PARAM(team_show_judge_report, "d"),
  GLOBAL_PARAM(disable_clars, "d"),
  GLOBAL_PARAM(disable_team_clars, "d"),
  GLOBAL_PARAM(max_file_length, "d"),
  GLOBAL_PARAM(max_line_length, "d"),
  GLOBAL_PARAM(tests_to_accept, "d"),
  GLOBAL_PARAM(ignore_compile_errors, "d"),
  GLOBAL_PARAM(inactivity_timeout, "d"),
  GLOBAL_PARAM(disable_auto_testing, "d"),
  GLOBAL_PARAM(disable_testing, "d"),

  GLOBAL_PARAM(charset, "s"),
  //GLOBAL_PARAM(standings_charset, "s"),

  GLOBAL_PARAM(root_dir, "s"),
  GLOBAL_PARAM(conf_dir, "s"),
  GLOBAL_PARAM(script_dir, "s"),
  GLOBAL_PARAM(test_dir, "s"),
  GLOBAL_PARAM(corr_dir, "s"),
  GLOBAL_PARAM(info_dir, "s"),
  GLOBAL_PARAM(tgz_dir, "s"),
  GLOBAL_PARAM(checker_dir, "s"),
  GLOBAL_PARAM(test_sfx, "s"),
  GLOBAL_PARAM(corr_sfx, "s"),
  GLOBAL_PARAM(info_sfx, "s"),
  GLOBAL_PARAM(tgz_sfx, "s"),
  GLOBAL_PARAM(ejudge_checkers_dir, "s"),
  GLOBAL_PARAM(test_pat, "s"),
  GLOBAL_PARAM(corr_pat, "s"),
  GLOBAL_PARAM(info_pat, "s"),
  GLOBAL_PARAM(tgz_pat, "s"),
  GLOBAL_PARAM(contest_start_cmd, "s"),

  GLOBAL_PARAM(var_dir, "s"),

  GLOBAL_PARAM(contest_id, "d"),
  GLOBAL_PARAM(socket_path, "s"),
  GLOBAL_PARAM(contests_dir, "s"),
  GLOBAL_PARAM(serve_socket, "s"),

  //GLOBAL_PARAM(log_file, "s"),
  GLOBAL_PARAM(run_log_file, "s"),
  GLOBAL_PARAM(clar_log_file, "s"),
  GLOBAL_PARAM(archive_dir, "s"),
  GLOBAL_PARAM(clar_archive_dir, "s"),
  GLOBAL_PARAM(run_archive_dir, "s"),
  GLOBAL_PARAM(report_archive_dir, "s"),
  GLOBAL_PARAM(team_report_archive_dir, "s"),
  GLOBAL_PARAM(team_extra_dir, "s"),

  GLOBAL_PARAM(status_dir, "s"),
  GLOBAL_PARAM(work_dir, "s"),
  GLOBAL_PARAM(print_work_dir, "s"),
  GLOBAL_PARAM(diff_work_dir, "s"),

  GLOBAL_PARAM(a2ps_path, "s"),
  GLOBAL_PARAM(a2ps_args, "x"),
  GLOBAL_PARAM(lpr_path, "s"),
  GLOBAL_PARAM(lpr_args, "x"),
  GLOBAL_PARAM(diff_path, "s"),

  GLOBAL_PARAM(compile_dir, "s"),
  GLOBAL_PARAM(compile_work_dir, "s"),

  GLOBAL_PARAM(run_dir, "s"),
  GLOBAL_PARAM(run_work_dir, "s"),
  GLOBAL_PARAM(run_check_dir, "s"),

  GLOBAL_PARAM(score_system, "s"),
  GLOBAL_PARAM(rounding_mode, "s"),
  GLOBAL_PARAM(virtual, "d"),

  GLOBAL_PARAM(htdocs_dir, "s"),

  GLOBAL_PARAM(team_info_url, "s"),
  GLOBAL_PARAM(prob_info_url, "s"),
  GLOBAL_PARAM(standings_file_name, "s"),
  GLOBAL_PARAM(stand_header_file, "s"),
  GLOBAL_PARAM(stand_footer_file, "s"),
  GLOBAL_PARAM(stand_symlink_dir, "s"),
  GLOBAL_PARAM(stand2_file_name, "s"),
  GLOBAL_PARAM(stand2_header_file, "s"),
  GLOBAL_PARAM(stand2_footer_file, "s"),
  GLOBAL_PARAM(stand2_symlink_dir, "s"),
  GLOBAL_PARAM(plog_file_name, "s"),
  GLOBAL_PARAM(plog_header_file, "s"),
  GLOBAL_PARAM(plog_footer_file, "s"),
  GLOBAL_PARAM(plog_update_time, "d"),
  GLOBAL_PARAM(plog_symlink_dir, "s"),

  // standings table attributes
  GLOBAL_PARAM(stand_extra_format, "s"),
  GLOBAL_PARAM(stand_extra_legend, "s"),
  GLOBAL_PARAM(stand_extra_attr, "s"),
  GLOBAL_PARAM(stand_table_attr, "s"),
  GLOBAL_PARAM(stand_place_attr, "s"),
  GLOBAL_PARAM(stand_team_attr, "s"),
  GLOBAL_PARAM(stand_prob_attr, "s"),
  GLOBAL_PARAM(stand_solved_attr, "s"),
  GLOBAL_PARAM(stand_score_attr, "s"),
  GLOBAL_PARAM(stand_penalty_attr, "s"),
  GLOBAL_PARAM(stand_time_attr, "s"),
  GLOBAL_PARAM(stand_self_row_attr, "s"),
  GLOBAL_PARAM(stand_v_row_attr, "s"),
  GLOBAL_PARAM(stand_r_row_attr, "s"),
  GLOBAL_PARAM(stand_u_row_attr, "s"),
  GLOBAL_PARAM(stand_success_attr, "s"),
  GLOBAL_PARAM(stand_show_ok_time, "s"),

  // just for fun
  GLOBAL_PARAM(extended_sound, "d"),
  GLOBAL_PARAM(sound_player, "s"),
  GLOBAL_PARAM(accept_sound, "s"),
  GLOBAL_PARAM(runtime_sound, "s"),
  GLOBAL_PARAM(timelimit_sound, "s"),
  GLOBAL_PARAM(wrong_sound, "s"),
  GLOBAL_PARAM(presentation_sound, "s"),
  GLOBAL_PARAM(internal_sound, "s"),
  GLOBAL_PARAM(start_sound, "s"),

  GLOBAL_PARAM(enable_l10n, "d"),
  GLOBAL_PARAM(l10n_dir, "s"),
  GLOBAL_PARAM(standings_locale, "s"),

  GLOBAL_PARAM(team_download_time, "d"),

  GLOBAL_PARAM(cr_serialization_key, "d"),
  GLOBAL_PARAM(show_astr_time, "d"),
  GLOBAL_PARAM(ignore_duplicated_runs, "d"),
  GLOBAL_PARAM(report_error_code, "d"),
  GLOBAL_PARAM(auto_short_problem_name, "d"),
  GLOBAL_PARAM(enable_continue, "d"),
  GLOBAL_PARAM(checker_real_time_limit, "d"),
  GLOBAL_PARAM(compile_real_time_limit, "d"),
  GLOBAL_PARAM(show_deadline, "d"),
  GLOBAL_PARAM(enable_runlog_merge, "d"),
  GLOBAL_PARAM(prune_empty_users, "d"),
  GLOBAL_PARAM(enable_report_upload, "d"),

  GLOBAL_PARAM(use_gzip, "d"),
  GLOBAL_PARAM(min_gzip_size, "d"),
  GLOBAL_PARAM(use_dir_hierarchy, "d"),
  GLOBAL_PARAM(html_report, "d"),

  GLOBAL_PARAM(variant_map_file, "s"),

  GLOBAL_PARAM(enable_printing, "d"),
  GLOBAL_PARAM(team_page_quota, "d"),

  GLOBAL_PARAM(priority_adjustment, "d"),
  GLOBAL_PARAM(user_priority_adjustments, "x"),

  GLOBAL_PARAM(contestant_status_num, "d"),
  GLOBAL_PARAM(contestant_status_legend, "x"),
  GLOBAL_PARAM(contestant_status_row_attr, "x"),
  GLOBAL_PARAM(stand_show_contestant_status, "d"),
  GLOBAL_PARAM(stand_show_warn_number, "d"),
  GLOBAL_PARAM(stand_contestant_status_attr, "s"),
  GLOBAL_PARAM(stand_warn_number_attr, "s"),

  { 0, 0, 0, 0 }
};

#define PROBLEM_OFFSET(x)   XOFFSET(struct section_problem_data, x)
#define PROBLEM_SIZE(x)     XFSIZE(struct section_problem_data, x)
#define PROBLEM_PARAM(x, t) { #x, t, PROBLEM_OFFSET(x), PROBLEM_SIZE(x) }
static struct config_parse_info section_problem_params[] =
{
  PROBLEM_PARAM(id, "d"),
  PROBLEM_PARAM(tester_id, "d"),
  PROBLEM_PARAM(abstract, "d"),
  PROBLEM_PARAM(use_stdin, "d"),
  PROBLEM_PARAM(use_stdout, "d"),
  PROBLEM_PARAM(time_limit, "d"),
  PROBLEM_PARAM(real_time_limit, "d"),
  PROBLEM_PARAM(team_enable_rep_view, "d"),
  PROBLEM_PARAM(team_enable_ce_view, "d"),
  PROBLEM_PARAM(team_show_judge_report, "d"),
  PROBLEM_PARAM(full_score, "d"),
  PROBLEM_PARAM(test_score, "d"),
  PROBLEM_PARAM(run_penalty, "d"),
  PROBLEM_PARAM(disqualified_penalty, "d"),
  PROBLEM_PARAM(use_corr, "d"),
  PROBLEM_PARAM(use_info, "d"),
  PROBLEM_PARAM(use_tgz, "d"),
  PROBLEM_PARAM(tests_to_accept, "d"),
  PROBLEM_PARAM(accept_partial, "d"),
  PROBLEM_PARAM(checker_real_time_limit, "d"),
  PROBLEM_PARAM(disable_auto_testing, "d"),
  PROBLEM_PARAM(disable_testing, "d"),
  PROBLEM_PARAM(variable_full_score, "d"),
  PROBLEM_PARAM(hidden, "d"),
  PROBLEM_PARAM(priority_adjustment, "d"),
  PROBLEM_PARAM(spelling, "s"),
  PROBLEM_PARAM(stand_hide_time, "d"),
  PROBLEM_PARAM(score_multiplier, "d"),

  PROBLEM_PARAM(super, "s"),
  PROBLEM_PARAM(short_name, "s"),
  PROBLEM_PARAM(long_name, "s"),
  PROBLEM_PARAM(test_dir, "s"),
  PROBLEM_PARAM(test_sfx, "s"),
  PROBLEM_PARAM(corr_dir, "s"),
  PROBLEM_PARAM(corr_sfx, "s"),
  PROBLEM_PARAM(info_dir, "s"),
  PROBLEM_PARAM(info_sfx, "s"),
  PROBLEM_PARAM(tgz_dir, "s"),
  PROBLEM_PARAM(tgz_sfx, "s"),
  PROBLEM_PARAM(input_file, "s"),
  PROBLEM_PARAM(output_file, "s"),
  PROBLEM_PARAM(test_score_list, "s"),
  PROBLEM_PARAM(test_sets, "x"),
  PROBLEM_PARAM(deadline, "s"),
  PROBLEM_PARAM(start_date, "s"),
  PROBLEM_PARAM(variant_num, "d"),
  PROBLEM_PARAM(date_penalty, "x"),
  PROBLEM_PARAM(disable_language, "x"),
  PROBLEM_PARAM(standard_checker, "s"),
  PROBLEM_PARAM(checker_env, "x"),
  PROBLEM_PARAM(test_pat, "s"),
  PROBLEM_PARAM(corr_pat, "s"),
  PROBLEM_PARAM(info_pat, "s"),
  PROBLEM_PARAM(tgz_pat, "s"),
  PROBLEM_PARAM(personal_deadline, "x"),

  { 0, 0, 0, 0 }
};

#define LANGUAGE_OFFSET(x)   XOFFSET(struct section_language_data, x)
#define LANGUAGE_SIZE(x)     XFSIZE(struct section_language_data, x)
#define LANGUAGE_PARAM(x, t) { #x, t, LANGUAGE_OFFSET(x), LANGUAGE_SIZE(x) }
static struct config_parse_info section_language_params[] =
{
  LANGUAGE_PARAM(id, "d"),
  LANGUAGE_PARAM(compile_id, "d"),
  LANGUAGE_PARAM(disabled, "d"),
  LANGUAGE_PARAM(binary, "d"),
  LANGUAGE_PARAM(priority_adjustment, "d"),
  LANGUAGE_PARAM(short_name, "s"),
  LANGUAGE_PARAM(long_name, "s"),
  LANGUAGE_PARAM(key, "s"),
  LANGUAGE_PARAM(arch, "s"),
  LANGUAGE_PARAM(src_sfx, "s"),
  LANGUAGE_PARAM(exe_sfx, "s"),
  LANGUAGE_PARAM(cmd, "s"),

  LANGUAGE_PARAM(disable_auto_testing, "d"),
  LANGUAGE_PARAM(disable_testing, "d"),

  LANGUAGE_PARAM(compile_dir, "s"),
  LANGUAGE_PARAM(compile_real_time_limit, "d"),
  LANGUAGE_PARAM(compiler_env, "x"),

  { 0, 0, 0, 0 }
};

#define TESTER_OFFSET(x) XOFFSET(struct section_tester_data, x)
#define TESTER_SIZE(x)   XFSIZE(struct section_tester_data, x)
#define TESTER_PARAM(x, t) { #x, t, TESTER_OFFSET(x), TESTER_SIZE(x) }
static struct config_parse_info section_tester_params[] =
{
  TESTER_PARAM(id, "d"),
  TESTER_PARAM(name, "s"),
  TESTER_PARAM(problem, "d"),
  TESTER_PARAM(problem_name, "s"),
  TESTER_PARAM(no_redirect, "d"),
  TESTER_PARAM(is_dos, "d"),
  TESTER_PARAM(arch, "s"),
  TESTER_PARAM(key, "s"),
  TESTER_PARAM(any, "d"),
  TESTER_PARAM(priority_adjustment, "d"),

  TESTER_PARAM(abstract, "d"),
  TESTER_PARAM(super, "x"),

  TESTER_PARAM(no_core_dump, "d"),
  TESTER_PARAM(kill_signal, "s"),
  TESTER_PARAM(max_stack_size, "d"),
  TESTER_PARAM(max_data_size, "d"),
  TESTER_PARAM(max_vm_size, "d"),
  TESTER_PARAM(clear_env, "d"),
  TESTER_PARAM(time_limit_adjustment, "d"),

  TESTER_PARAM(run_dir, "s"),
  TESTER_PARAM(check_dir, "s"),
  TESTER_PARAM(errorcode_file, "s"),
  TESTER_PARAM(error_file, "s"),

  TESTER_PARAM(prepare_cmd, "s"),
  TESTER_PARAM(check_cmd, "s"),
  TESTER_PARAM(start_cmd, "s"),

  TESTER_PARAM(start_env, "x"),
  TESTER_PARAM(checker_env, "x"),

  { 0, 0, 0, 0 }
};

static int problem_counter;
static int language_counter;
static int tester_counter;

static void problem_init_func(struct generic_section_config *);
static void tester_init_func(struct generic_section_config *);
static void global_init_func(struct generic_section_config *);
static void language_init_func(struct generic_section_config *);

static struct config_section_info params[] =
{
  { "global", sizeof(struct section_global_data), section_global_params,
    0, global_init_func },
  { "problem", sizeof(struct section_problem_data), section_problem_params,
    &problem_counter, problem_init_func },
  { "language",sizeof(struct section_language_data),section_language_params,
    &language_counter, language_init_func },
  { "tester", sizeof(struct section_tester_data), section_tester_params,
    &tester_counter, tester_init_func },
  { NULL, 0, NULL }
};

int
find_tester(int problem, char const *arch)
{
  int i;

  for (i = 1; i <= max_tester; i++) {
    if (!testers[i]) continue;
    if (testers[i]->any) continue;
    if (problem == testers[i]->problem
        && !strcmp(arch, testers[i]->arch))
      return i;
  }
  for (i = 1; i <= max_tester; i++) {
    if (!testers[i]) continue;
    if (testers[i]->any && !strcmp(arch, testers[i]->arch))
      return i;
  }
  
  return 0;
}

#define DFLT_G_SLEEP_TIME         1000
#define DFLT_G_SERVE_SLEEP_TIME   100
#define DFLT_G_MAX_RUN_SIZE       65536
#define DFLT_G_MAX_RUN_TOTAL      (2 * 1024 * 1024)
#define DFLT_G_MAX_RUN_NUM        200
#define DFLT_G_MAX_CLAR_SIZE      1024
#define DFLT_G_MAX_CLAR_TOTAL     (40 * 1024)
#define DFLT_G_MAX_CLAR_NUM       50
#define DFLT_G_BOARD_FOG_TIME     60
#define DFLT_G_BOARD_UNFOG_TIME   120
#define DFLT_G_CONTEST_TIME       60
#define DFLT_G_ROOT_DIR           "contest"
#define DFLT_G_CONF_DIR           "conf"
#define DFLT_G_VAR_DIR            "var"
#define DFLT_G_SCRIPT_DIR         "scripts"
#define DFLT_G_TEST_DIR           "tests"
#define DFLT_G_CORR_DIR           "correct"
#define DFLT_G_INFO_DIR           "info"
#define DFLT_G_INFO_SFX           ".inf"
#define DFLT_G_TGZ_DIR            "info"
#define DFLT_G_TGZ_SFX            ".tgz"
#define DFLT_G_CHECKER_DIR        "checkers"
#define DFLT_G_RUN_LOG_FILE       "run.log"
#define DFLT_G_CLAR_LOG_FILE      "clar.log"
#define DFLT_G_ARCHIVE_DIR        "archive"
#define DFLT_G_CLAR_ARCHIVE_DIR   "clars"
#define DFLT_G_RUN_ARCHIVE_DIR    "runs"
#define DFLT_G_REPORT_ARCHIVE_DIR "reports"
#define DFLT_G_TEAM_REPORT_ARCHIVE_DIR "teamreports"
#define DFLT_G_TEAM_EXTRA_DIR     "team_extra"
#define DFLT_G_PIPE_DIR           "pipe"
#define DFLT_G_TEAM_DIR           "team"
#define DFLT_G_TEAM_CMD_DIR       "cmd"
#define DFLT_G_TEAM_DATA_DIR      "data"
#define DFLT_G_JUDGE_DIR          "judge"
#define DFLT_G_JUDGE_CMD_DIR      "cmd"
#define DFLT_G_JUDGE_DATA_DIR     "data"
#define DFLT_G_STATUS_DIR         "status"
#define DFLT_G_WORK_DIR           "work"
#define DFLT_G_PRINT_WORK_DIR     "print"
#define DFLT_G_DIFF_WORK_DIR      "diff"
#define DFLT_G_A2PS_PATH          "/usr/bin/a2ps"
#define DFLT_G_LPR_PATH           "/usr/bin/lpr"
#define DFLT_G_DIFF_PATH          "/usr/bin/diff"
#define DFLT_G_COMPILE_DIR        "compile"
#define DFLT_G_COMPILE_QUEUE_DIR  "queue"
#define DFLT_G_COMPILE_SRC_DIR    "src"
#define DFLT_G_COMPILE_STATUS_DIR "status"
#define DFLT_G_COMPILE_REPORT_DIR "report"
#define DFLT_G_COMPILE_WORK_DIR   "compile"
#define DFLT_G_RUN_DIR            "run"
#define DFLT_G_RUN_QUEUE_DIR      "queue"
#define DFLT_G_RUN_EXE_DIR        "exe"
#define DFLT_G_RUN_STATUS_DIR     "status"
#define DFLT_G_RUN_REPORT_DIR     "report"
#define DFLT_G_RUN_TEAM_REPORT_DIR "teamreport"
#define DFLT_G_RUN_WORK_DIR       "runwork"
#define DFLT_G_RUN_CHECK_DIR      "runcheck"

#if defined EJUDGE_CHARSET
#define DFLT_G_CHARSET            EJUDGE_CHARSET
#else
#define DFLT_G_CHARSET            "iso8859-1"
#endif /* EJUDGE_CHARSET */

#define DFLT_G_STANDINGS_FILE_NAME "standings.html"
#define DFLT_G_MAX_FILE_LENGTH    65535
#define DFLT_G_MAX_LINE_LENGTH    4096
#define DFLT_G_MAX_CMD_LENGTH     256
#define DFLT_G_TEAM_DOWNLOAD_TIME 30
#define DFLT_G_SERVE_SOCKET       "serve"
#define DFLT_G_INACTIVITY_TIMEOUT 120
#define DFLT_G_CHECKER_REAL_TIME_LIMIT 30
#define DFLT_G_COMPILE_REAL_TIME_LIMIT 30
#define DFLT_G_USE_GZIP          1
#define DFLT_G_USE_DIR_HIERARCHY 1
#define DFLT_G_MIN_GZIP_SIZE     4096
#define DFLT_G_TEAM_PAGE_QUOTA   50

#define DFLT_P_INPUT_FILE         "input"
#define DFLT_P_OUTPUT_FILE        "output"
#define DFLT_P_FULL_SCORE         25
#define DFLT_P_TEST_SCORE         1
#define DFLT_P_RUN_PENALTY        1
#define DFLT_P_VARIABLE_FULL_SCORE 0
#define DFLT_P_HIDDEN             0

#define DFLT_T_WORK_DIR           "work"
#define DFLT_T_TMP_DIR            "tmp"
#define DFLT_T_ERROR_FILE         "error"

static void
global_init_func(struct generic_section_config *gp)
{
  struct section_global_data *p = (struct section_global_data *) gp;

  p->autoupdate_standings = 1;
  p->board_fog_time = -1;
  p->board_unfog_time = -1;
  p->contest_time = -1;
  p->tests_to_accept = -1;
  p->team_download_time = -1;
  p->ignore_duplicated_runs = -1;
  p->inactivity_timeout = -1;
  p->checker_real_time_limit = -1;
  p->compile_real_time_limit = -1;
  p->use_gzip = -1;
  p->use_dir_hierarchy = -1;
  p->min_gzip_size = -1;
  p->team_page_quota = -1;
  p->enable_l10n = -1;
}

static void
language_init_func(struct generic_section_config *gp)
{
  struct section_language_data *p = (struct section_language_data*) gp;

  p->compile_real_time_limit = -1;
}

static void
problem_init_func(struct generic_section_config *gp)
{
  struct section_problem_data *p = (struct section_problem_data*) gp;

  p->use_stdin = -1;
  p->use_stdout = -1;
  p->team_enable_rep_view = -1;
  p->team_enable_ce_view = -1;
  p->team_show_judge_report = -1;
  p->use_corr = -1;
  p->use_info = -1;
  p->use_tgz = -1;
  p->tests_to_accept = -1;
  p->accept_partial = -1;
  p->test_sfx[0] = 1;
  p->corr_sfx[0] = 1;
  p->info_sfx[0] = 1;
  p->tgz_sfx[0] = 1;
  p->run_penalty = -1;
  p->disqualified_penalty = -1;
  p->checker_real_time_limit = -1;
  p->variant_num = -1;
  p->disable_auto_testing = -1;
  p->disable_testing = -1;
  p->test_score = -1;
  p->variable_full_score = -1;
  p->hidden = -1;
  p->priority_adjustment = -1000;
  p->test_pat[0] = 1;
  p->corr_pat[0] = 1;
  p->info_pat[0] = 1;
  p->tgz_pat[0] = 1;
}

static void
tester_init_func(struct generic_section_config *gp)
{
  struct section_tester_data *p = (struct section_tester_data*) gp;

  p->is_dos = -1;
  p->no_redirect = -1;
  p->no_core_dump = -1;
  p->clear_env = -1;
  p->time_limit_adjustment = -1;
  p->priority_adjustment = -1000;
}

static char*
tester_get_name(void const *vpt)
{
  struct section_tester_data *pt = (struct section_tester_data *) vpt;
  return pt->name;
}

struct inheritance_info
{
  unsigned long  offset;        /* offset of this field */
  char          *name;          /* name of this field */

  int (*isdef_func)(void *);    /* checks, whether field is defined */
  void (*copy_func)(void *d, void *s); /* copies s to d */
};

int
inherit_fields(struct inheritance_info *iinfo,
               void *obj, char *name, int stot, void **sups,
               char *(*get_name_func)(void const *))
{
  int   ii, j, defnum, defpos;
  void *objf, *sobjf;

  for (ii = 0; iinfo[ii].name; ii++) {
    objf = XPDEREF(void, obj, iinfo[ii].offset);
    /*
    fprintf(stderr, ">>objf: %#08lx,%lu,%#08lx\n",
            (unsigned long) obj, (unsigned long) iinfo[ii].offset,
            (unsigned long) objf);
    */
    if (iinfo[ii].isdef_func(objf)) continue;
    for (j = 0, defpos = -1, defnum = 0; j < stot; j++) {
      sobjf = XPDEREF(void, sups[j], iinfo[ii].offset);
      if (iinfo[ii].isdef_func(sobjf)) {
        defnum++;
        defpos = j;
      }
    }
    if (defnum > 1) {
      err("several supertesters define %s for %s",
          iinfo[ii].name, name);
      return -1;
    }
    if (defnum == 0) continue;
    sobjf = XPDEREF(void, sups[defpos], iinfo[ii].offset);
    info("%s.%s inherited from %s",
         name, iinfo[ii].name, get_name_func(sups[defpos]));
    iinfo[ii].copy_func(objf, sobjf);
  }

  return 0;
}

static int inh_isdef_int(void *vpint)
{
  int *pint = (int*) vpint;
  if (*pint != -1) return 1;
  return 0;
}
static int inh_isdef_int2(void *vpint)
{
  int *pint = (int*) vpint;
  if (*pint != 0) return 1;
  return 0;
}
static int inh_isdef_int3(void *vpint)
{
  int *pint = (int*) vpint;
  if (*pint != -1000) return 1;
  return 0;
}
static void inh_copy_int(void *dst, void *src)
{
  memcpy(dst, src, sizeof(int));
}

static int inh_isdef_path(void *vppath)
{
  char *pc = (char *) vppath;
  if (*pc) return 1;
  return 0;
}
static int inh_isdef_path2(void *vppath)
{
  char *pc = (char *) vppath;

  (void) &(inh_isdef_path2);
  if (*pc == 1) return 0;
  return 1;
}
static void inh_copy_path(void *dst, void *src)
{
  memcpy(dst, src, sizeof(path_t));
}

#define TESTER_INH(f,d,c) {TESTER_OFFSET(f),#f,inh_isdef_##d,inh_copy_##c }
static struct inheritance_info tester_inheritance_info[] =
{
  TESTER_INH(arch, path, path),
  TESTER_INH(key, path, path),
  TESTER_INH(run_dir, path, path),
  TESTER_INH(no_core_dump, int, int),
  TESTER_INH(clear_env, int, int),
  TESTER_INH(time_limit_adjustment, int, int),
  TESTER_INH(kill_signal, path, path),
  TESTER_INH(max_stack_size, int2, int),
  TESTER_INH(max_data_size, int2, int),
  TESTER_INH(max_vm_size, int2, int),
  TESTER_INH(is_dos, int, int),
  TESTER_INH(no_redirect, int, int),
  TESTER_INH(priority_adjustment, int3, int),
  TESTER_INH(check_dir, path, path),
  TESTER_INH(errorcode_file, path, path),
  TESTER_INH(error_file, path, path),
  TESTER_INH(check_cmd, path, path),
  TESTER_INH(start_cmd, path, path),
  TESTER_INH(prepare_cmd, path, path),

  { 0, 0, 0, 0 }
};

static int
process_abstract_tester(int i)
{
  struct section_tester_data *atp = abstr_testers[i], *katp;
  struct section_tester_data **sups;
  char ***envs;
  char *ish;
  char **nenv;
  int   stot, j, k;

  if (!atp->name[0]) {
    err("abstract tester must define tester name");
    return -1;
  }
  if (atp->any) {
    err("abstract tester cannot be default");
    return -1;
  }
  ish = atp->name;
  if (atp->id) {
    err("abstract tester %s must not have id", ish);
    return -1;
  }
  if (atp->problem || atp->problem_name[0]) {
    err("abstract tester %s cannot reference a problem", ish);
    return -1;
  }

  // no inheritance
  if (!atp->super || !atp->super[0]) {
    atp->is_processed = 1;
    return 0;
  }

  // count the number of supertesters and create array of references
  for (stot = 0; atp->super[stot]; stot++);
  sups = (struct section_tester_data**) alloca(stot * sizeof(sups[0]));
  envs = (char***) alloca((stot + 1) * sizeof(envs[0]));
  memset(sups, 0, stot * sizeof(sups[0]));
  memset(envs, 0, stot * sizeof(envs[0]));
  envs[stot] = atp->start_env;

  for (j = 0; j < stot; j++) {
    katp = 0;
    for (k = 0; k < max_abstr_tester; k++) {
      katp = abstr_testers[k];
      if (!katp || !katp->name[0]) continue;
      if (!strcmp(atp->super[j], katp->name)) break;
    }
    if (k >= max_abstr_tester || !katp) {
      err("abstract tester %s not found", atp->super[j]);
      return -1;
    }
    if (!katp->is_processed) {
      err("abstract tester %s must be defined before use", atp->super[j]);
      return -1;
    }
    sups[j] = katp;
    envs[j] = katp->start_env;
  }

  for (j = 0; j < stot; j++)
    fprintf(stderr, ">>%s\n", sups[j]->name);

  if (inherit_fields(tester_inheritance_info,
                     atp, ish, stot, (void**) sups,
                     tester_get_name) < 0)
    return -1;

  // merge all the start_env fields
  nenv = sarray_merge_arr(stot + 1, envs);
  sarray_free(atp->start_env);
  atp->start_env = nenv;

  atp->is_processed = 1;
  return 0;
}

static int
parse_testsets(char **set_in, int *p_total, struct testset_info **p_info)
{
  int total = 0;
  struct testset_info *info = 0;
  int i, n, x, t, score;
  unsigned char *s;

  if (!set_in || !set_in[0]) return 0;

  *p_total = 0;
  *p_info = 0;

  for (total = 0; set_in[total]; total++);
  info = (struct testset_info*) xcalloc(total, sizeof(info[0]));

  for (i = 0; i < total; i++) {
    s = set_in[i];
    t = -1;
    while (1) {
      n = 0;
      if (sscanf(s, "%d%n", &x, &n) != 1) break;
      if (x <= 0 || x >= 1000) {
        err("invalid test number in testset specification");
        return -1;
      }
      if (x > t) t = x;
      s += n;
    }

    if (t == -1) {
      err("no test defined in testset");
      return -1;
    }

    while (isspace(*s)) s++;
    if (*s != '=') {
      err("`=' expected in the testset specification");
      return -1;
    }
    s++;

    n = 0;
    if (sscanf(s, "%d %n", &score, &n) != 1) {
      err("score expected in the testset specification");
      return -1;
    }
    if (s[n]) {
      err("garbage after testset specification");
      return -1;
    }
    if (score < 0) {
      err("invalid score in testset specification");
      return -1;
    }

    info[i].total = t;
    info[i].score = score;
    info[i].testop = 0;
    info[i].scoreop = 0;
    info[i].nums = (unsigned char*) xcalloc(t, sizeof(info[i].nums[0]));

    s = set_in[i];
    while (1) {
      n = 0;
      if (sscanf(s, "%d%n", &x, &n) != 1) break;
      ASSERT(x > 0 && x < 1000);
      s += n;
      info[i].nums[x - 1] = 1;
    }
    /*
    fprintf(stderr, ">>%d\n", t);
    for (n = 0; n < t; n++)
      fprintf(stderr, " %d", info[i].nums[n]);
    fprintf(stderr, "\n");
    */
  }

  *p_info = info;
  *p_total = total;
  return 0;
}

static int
parse_date(const unsigned char *s, time_t *pd)
{
  int year, month, day, hour, min, sec, n;
  time_t t;
  struct tm tt;

  if (!s) goto failed;

  memset(&tt, 0, sizeof(tt));
  while (1) {
    year = month = day = hour = min = sec = 0;
    if (sscanf(s, "%d/%d/%d %d:%d:%d %n", &year, &month, &day, &hour,
               &min, &sec, &n) == 6 && !s[n]) break;
    sec = 0;
    if (sscanf(s, "%d/%d/%d %d:%d %n", &year, &month, &day, &hour, &min, &n)
        == 5 && !s[n]) break;
    min = sec = 0;
    if (sscanf(s, "%d/%d/%d %d %n", &year, &month, &day, &hour, &n) == 4
        && !s[n]) break;
    hour = min = sec = 0;
    if (sscanf(s, "%d/%d/%d %n", &year, &month, &day, &n) == 3 && !s[n]) break;
    goto failed;
  }

  if (year < 1900 || year > 2100 || month < 1 || month > 12
      || day < 1 || day > 31 || hour < 0 || hour >= 24
      || min < 0 || min >= 60 || sec < 0 || sec >= 60) goto failed;
  tt.tm_sec = sec;
  tt.tm_min = min;
  tt.tm_hour = hour;
  tt.tm_mday = day;
  tt.tm_mon = month - 1;
  tt.tm_year = year - 1900;
  if ((t = mktime(&tt)) == (time_t) -1) goto failed;
  *pd = t;
  return 0;

 failed:
  return -1;
}

static int
parse_deadline_penalties(char **dpstr, int *p_total,
                         struct penalty_info **p_pens)
{
  int total = 0, i, n, x;
  struct penalty_info *v = 0;
  const char *s;
  size_t maxlen = 0, curlen;
  unsigned char *b1, *b2, *b3;
  time_t tt;

  *p_total = 0;
  *p_pens = 0;
  if (!dpstr || !*dpstr) return 0;

  for (i = 0; dpstr[i]; i++) {
    curlen = strlen(dpstr[i]);
    if (curlen > maxlen) maxlen = curlen;
    total++;
  }
  if (!total) return 0;
  XCALLOC(v, total);
  b1 = (unsigned char*) alloca(maxlen + 10);
  b2 = (unsigned char*) alloca(maxlen + 10);
  b3 = (unsigned char*) alloca(maxlen + 10);

  for (i = 0; (s = dpstr[i]); i++) {
    if (sscanf(s, "%s%s%s%n", b1, b3, b2, &n) == 3 && !s[n]) {
      strcat(b1, " ");
      strcat(b1, b3);
    } else if (sscanf(s, "%s%s%n", b1, b2, &n) == 2 && !s[n]) {
      // do nothing
    } else if (sscanf(s, "%s%n", b2, &n) == 1 && !s[n]) {
      if (maxlen + 10 < 64) b1 = (unsigned char*) alloca(64);
      strcpy(b1, "2038/01/19");
    }  else {
      err("%d: invalid date penalty specification %s", i + 1, s);
      goto failure;
    }
    n = x = 0;
    if (sscanf(b2, "%d%n", &x, &n) != 1 || b2[n]) {
      err("%d: invalid penalty specification %s", i + 1, b2);
      goto failure;
    }
    if (parse_date(b1, &tt) < 0) {
      err("%d: invalid date specification %s", i + 1, b1);
      goto failure;
    }
    v[i].deadline = tt;
    v[i].penalty = x;
  }

  /*
  fprintf(stderr, ">>Total %d\n", total);
  for (i = 0; i < total; i++)
    fprintf(stderr, ">>[%d]: %ld,%d\n", i + 1, v[i].deadline, v[i].penalty);
  */

  *p_total = total;
  *p_pens = v;
  return 0;

 failure:
  xfree(v);
  return -1;
}

/* login deadline */
static int
parse_personal_deadlines(char **pdstr, int *p_total,
                         struct pers_dead_info **p_dl)
{
  int total, i, maxlen = 0, n;
  struct pers_dead_info *dinfo;
  unsigned char *s1, *s2, *s3;

  for (total = 0; pdstr[total]; total++) {
    if ((i = strlen(pdstr[total])) > maxlen) maxlen = i;
  }

  if (!total) {
    *p_dl = 0;
    *p_total = 0;
    return 0;
  }

  XCALLOC(dinfo, total);
  s1 = alloca(maxlen + 16);
  s2 = alloca(maxlen + 16);
  s3 = alloca(maxlen + 16);

  for (i = 0; i < total; i++) {
    if (sscanf(pdstr[i], "%s%s%s%n", s1, s2, s3, &n) == 3 && !pdstr[i][n]) {
      strcat(s2, " ");
      strcat(s2, s3);
    } else if (!sscanf(pdstr[i], "%s%s%n", s1, s2, &n) == 2 && !pdstr[i][n]) {
    } else if (!sscanf(pdstr[i], "%s%n", s1, &n) == 1 && !pdstr[i][n]) {
      strcpy(s2, "2038/01/19");
    }

    if (parse_date(s2, &dinfo[i].deadline) < 0) {
      err("%d: invalid date specification %s", i + 1, s2);
      return -1;
    }
    dinfo[i].login = xstrdup(s1);
  }

  // debug
  /*
  fprintf(stderr, "personal deadlines:\n");
  for (i = 0; i < total; i++) {
    fprintf(stderr, "[%d] %s %ld\n", i, dinfo[i].login, dinfo[i].deadline);
  }
  */

  *p_dl = dinfo;
  *p_total = total;
  return i;
}

static struct variant_map *
parse_variant_map(const unsigned char *path)
{
  unsigned char buf[1200];
  unsigned char login_buf[sizeof(buf)];
  unsigned char *p;
  int vintage, n, len, i, j, v;
  FILE *f;
  struct variant_map *pmap;

  XCALLOC(pmap, 1);
  XCALLOC(pmap->prob_map, max_prob + 1);
  XCALLOC(pmap->prob_rev_map, max_prob + 1);
  pmap->var_prob_num = 0;
  for (i = 1; i <= max_prob; i++) {
    if (!probs[i] || probs[i]->variant_num <= 0) continue;
    pmap->prob_map[i] = ++pmap->var_prob_num;
    pmap->prob_rev_map[pmap->var_prob_num] = i;
  }
  pmap->a = 16;
  XCALLOC(pmap->v, pmap->a);

  if (!(f = fopen(path, "r"))) {
    err("Cannot open variant map file '%s'", path);
    return 0;
  }
  if (!fgets(buf, sizeof(buf), f)) {
    err("Unexpected EOF in variant map file '%s'", path);
    return 0;
  }
  len = strlen(buf);
  if (len > 1024) {
    err("Line is too long in '%s'", path);
    return 0;
  }
  while (len > 0 && isspace(buf[len - 1])) buf[--len] = 0;
  if (sscanf(buf, " < variant_map version = \"%d\" >%n",
             &vintage, &n) != 1 || buf[n]) {
    err("Invalid header of the variant map file '%s'", path);
    return 0;
  }
  if (vintage != 1) {
    err("Cannot handle variant map file '%s' version %d", path, vintage);
    return 0;
  }

  while (fgets(buf, sizeof(buf), f)) {
    if ((p = strchr(buf, '#'))) *p = 0;
    len = strlen(buf);
    if (len > 1024) {
      err("Line is too long in '%s'", path);
      return 0;
    }
    while (len > 0 && isspace(buf[len - 1])) buf[--len] = 0;
    if (!len) continue;
    if (!strcmp(buf, "</variant_map>")) break;

    if (pmap->u >= pmap->a) {
      pmap->a *= 2;
      pmap->v = (typeof(pmap->v)) xrealloc(pmap->v,
                                           pmap->a * sizeof(pmap->v[0]));
    }
    memset(&pmap->v[pmap->u], 0, sizeof(pmap->v[0]));
    XCALLOC(pmap->v[pmap->u].variants, pmap->var_prob_num + 1);

    p = buf;
    if (sscanf(p, "%s%n", login_buf, &n) != 1) {
      err("Cannot read team login");
      return 0;
    }
    p += n;
    pmap->v[pmap->u].login = xstrdup(login_buf);

    for (j = 1; j <= pmap->var_prob_num; j++) {
      i = pmap->prob_rev_map[j];
      ASSERT(i > 0 && i <= max_prob);
      ASSERT(probs[i]);
      ASSERT(probs[i]->variant_num > 0);
      if (sscanf(p, "%d%n", &v, &n) != 1) {
        err("Cannot read variant for team %s and problem %s",
            login_buf, probs[i]->short_name);
        return 0;
      }
      if (v <= 0 || v > probs[i]->variant_num) {
        err("Invalid variant %d for team %s and problem %s",
            v, login_buf, probs[i]->short_name);
        return 0;
      }
      p += n;
      pmap->v[pmap->u].variants[j] = v;
    }
    pmap->u++;
  }

  if (ferror(f)) {
    err("Input error from '%s'", path);
    return 0;
  }

#if 0
  fprintf(stderr, "Parsed variant map version %d\n", vintage);
  for (i = 0; i < pmap->u; i++) {
    fprintf(stderr, "%s: ", pmap->v[i].login);
    for (j = 1; j <= pmap->var_prob_num; j++)
      fprintf(stderr, "%s(%d) ",
              probs[pmap->prob_rev_map[j]]->short_name,
              pmap->v[i].variants[j]);
    fprintf(stderr, "\n");
  }
#endif

  fclose(f);
  return pmap;
}

static struct user_adjustment_info *
parse_user_adjustment(char **strs)
{
  int count = 0, i, x, n;
  struct user_adjustment_info *pinfo = 0;

  if (!strs) return 0;
  for (; strs[count]; count++);
  XCALLOC(pinfo, count + 1);
  for (i = 0; i < count; i++) {
    pinfo[i].login = xmalloc(strlen(strs[i]) + 1);
    n = 0;
    if (sscanf("%s %d %n", pinfo[i].login, &x, &n) != 2 || strs[i][n]) {
      err("invalid user adjustment line %d", i + 1);
      return 0;
    }
    if (x <= -1000 || x >= 1000) {
      err("user priority adjustment %d at line %d is invalid", x, i + 1);
      return 0;
    }
    pinfo[i].adjustment = x;
  }
  return pinfo;
}

static int
set_defaults(int mode)
{
  struct generic_section_config *p;

  int i, j, si;
  char *ish;
  char *sish;

  size_t tmp_len = 0;
  int r;

  /* find global section */
  for (p = config; p; p = p->next)
    if (!p->name[0] || !strcmp(p->name, "global"))
      break;
  if (!p) {
    err("Global configuration settings not found");
    return -1;
  }
  global = (struct section_global_data*) p;

  /* userlist-server interaction */
  if (mode == PREPARE_SERVE) {
#if defined EJUDGE_SOCKET_PATH
    if (!global->socket_path[0]) {
      snprintf(global->socket_path, sizeof(global->socket_path),
               "%s", EJUDGE_SOCKET_PATH);
    }
#endif /* EJUDGE_SOCKET_PATH */
    if (!global->socket_path[0]) {
      err("global.socket_path must be set");
      return -1;
    }
  }

  /* directory poll intervals */
  if (global->sleep_time < 0 || global->sleep_time > 10000) {
    err("Invalid global.sleep_time value");
    return -1;
  }
  if (mode == PREPARE_SERVE) {
    if (global->serve_sleep_time < 0 || global->serve_sleep_time > 10000) {
      err("Invalid global.serve_sleep_time value");
      return -1;
    }
  }
  if (!global->sleep_time && !global->serve_sleep_time) {
    info("global.sleep_time set to %d", DFLT_G_SLEEP_TIME);
    global->sleep_time = DFLT_G_SLEEP_TIME;
    if (mode == PREPARE_SERVE) {
      info("global.serve_sleep_time set to %d", DFLT_G_SERVE_SLEEP_TIME);
      global->serve_sleep_time = DFLT_G_SERVE_SLEEP_TIME;
    }
  } else if (!global->sleep_time) {
    info("global.sleep_time set to %d", DFLT_G_SLEEP_TIME);
    global->sleep_time = DFLT_G_SLEEP_TIME;
  } else if (mode == PREPARE_SERVE && !global->serve_sleep_time) {
    info("global.serve_sleep_time set to global.sleep_time");
    global->serve_sleep_time = global->sleep_time;
  }

#define GLOBAL_INIT_NUM_FIELD(f,v) do { if (!global->f) { info("global.%s set to %d", #f, v); global->f = v; } } while (0)
  /* limits (serve) */
  if (mode == PREPARE_SERVE) {
    GLOBAL_INIT_NUM_FIELD(max_run_size, DFLT_G_MAX_RUN_SIZE);
    GLOBAL_INIT_NUM_FIELD(max_run_num, DFLT_G_MAX_RUN_NUM);
    GLOBAL_INIT_NUM_FIELD(max_run_total, DFLT_G_MAX_RUN_TOTAL);
    GLOBAL_INIT_NUM_FIELD(max_clar_size, DFLT_G_MAX_CLAR_SIZE);
    GLOBAL_INIT_NUM_FIELD(max_clar_num, DFLT_G_MAX_CLAR_NUM);
    GLOBAL_INIT_NUM_FIELD(max_clar_total, DFLT_G_MAX_CLAR_TOTAL);
  }

  /* timings */
  if (mode == PREPARE_SERVE) {
    if (global->board_fog_time < 0) {
      info("global.board_fog_time set to %d", DFLT_G_BOARD_FOG_TIME);
      global->board_fog_time = DFLT_G_BOARD_FOG_TIME;
    }
    global->board_fog_time *= 60;
    if (global->board_unfog_time == -1) {
      info("global.board_unfog_time set to %d", DFLT_G_BOARD_UNFOG_TIME);
      global->board_unfog_time = DFLT_G_BOARD_UNFOG_TIME;
    }
    global->board_unfog_time *= 60;
    if (global->contest_time < -1) {
      err("bad value of global.contest_time: %d", global->contest_time);
      return -1;
    }
    if (global->contest_time == -1) {
      info("global.contest_time set to %d", DFLT_G_CONTEST_TIME);
      global->contest_time = DFLT_G_CONTEST_TIME;
    }
    global->contest_time *= 60;
  }

  if (mode == PREPARE_SERVE || mode == PREPARE_RUN) {
    if (global->inactivity_timeout == -1) {
      info("global.inactivity_timeout set to %d", DFLT_G_INACTIVITY_TIMEOUT);
      global->inactivity_timeout = DFLT_G_INACTIVITY_TIMEOUT;
    }
  }

  if (!global->root_dir[0]) {
    err("global.root_dir must be set!");
    return -1;
  }

  /* root_dir, conf_dir, var_dir */
  if (!global->root_dir[0] && !global->var_dir[0] && !global->conf_dir[0]) {
    info("global.root_dir set to %s", DFLT_G_ROOT_DIR);
    info("global.conf_dir set to %s", DFLT_G_CONF_DIR);
    info("global.var_dir set to %s", DFLT_G_VAR_DIR);
    snprintf(global->root_dir, sizeof(global->root_dir),
             "%s", DFLT_G_ROOT_DIR);
    path_init(global->conf_dir, global->root_dir, DFLT_G_CONF_DIR);
    path_init(global->var_dir, global->root_dir, DFLT_G_VAR_DIR);
  } else if (global->root_dir[0]) {
    if (!global->conf_dir[0]) {
      info("global.conf_dir set to %s", DFLT_G_CONF_DIR);
    }
    if (!global->var_dir[0]) {
      info("global.var_dir set to %s", DFLT_G_VAR_DIR);
    }
    path_init(global->conf_dir, global->root_dir, DFLT_G_CONF_DIR);
    path_init(global->var_dir, global->root_dir, DFLT_G_VAR_DIR);
  } else if (!global->var_dir[0]) {
    err("global.var_dir must be set!");
    return -1;
  } else if (!global->conf_dir[0]) {
    err("global.conf_dir must be set!");
    return -1;
  }

  /* CONFIGURATION FILES DEFAULTS */
#define GLOBAL_INIT_FIELD(f,d,c) do { if (!global->f[0]) { info("global." #f " set to %s", d); snprintf(global->f, sizeof(global->f), "%s", d); } pathmake2(global->f,global->c, "/", global->f, NULL); } while (0)

#if defined EJUDGE_SCRIPT_DIR
  if (!global->script_dir[0]) {
    snprintf(global->script_dir, sizeof(global->script_dir), "%s",
             EJUDGE_SCRIPT_DIR);
    info("global.script_dir is set to %s", global->script_dir);
  }
  if (!global->ejudge_checkers_dir[0]) {
    snprintf(global->ejudge_checkers_dir, sizeof(global->ejudge_checkers_dir),
             "%s", EJUDGE_SCRIPT_DIR);
    info("global.ejudge_checkers_dir is set to %s",
         global->ejudge_checkers_dir);
  }
#endif /* EJUDGE_SCRIPT_DIR */

  if (mode == PREPARE_RUN) {
    GLOBAL_INIT_FIELD(test_dir, DFLT_G_TEST_DIR, conf_dir);
    GLOBAL_INIT_FIELD(corr_dir, DFLT_G_CORR_DIR, conf_dir);
    GLOBAL_INIT_FIELD(info_dir, DFLT_G_INFO_DIR, conf_dir);
    GLOBAL_INIT_FIELD(tgz_dir, DFLT_G_TGZ_DIR, conf_dir);
    GLOBAL_INIT_FIELD(checker_dir, DFLT_G_CHECKER_DIR, conf_dir);

    if (!global->info_sfx[0]) {
      snprintf(global->info_sfx, sizeof(global->info_sfx),
               "%s", DFLT_G_INFO_SFX);
      info("global.info_sfx set to %s", global->info_sfx);
    }
    if (!global->tgz_sfx[0]) {
      snprintf(global->tgz_sfx, sizeof(global->tgz_sfx),
               "%s", DFLT_G_TGZ_SFX);
      info("global.tgz_sfx set to %s", global->tgz_sfx);
    }
  }

  if (mode == PREPARE_SERVE) {
    GLOBAL_INIT_FIELD(run_log_file, DFLT_G_RUN_LOG_FILE, var_dir);
    GLOBAL_INIT_FIELD(clar_log_file, DFLT_G_CLAR_LOG_FILE, var_dir);
    GLOBAL_INIT_FIELD(archive_dir, DFLT_G_ARCHIVE_DIR, var_dir);
    GLOBAL_INIT_FIELD(clar_archive_dir, DFLT_G_CLAR_ARCHIVE_DIR, archive_dir);
    GLOBAL_INIT_FIELD(run_archive_dir, DFLT_G_RUN_ARCHIVE_DIR, archive_dir);
    GLOBAL_INIT_FIELD(report_archive_dir,DFLT_G_REPORT_ARCHIVE_DIR,archive_dir);
    GLOBAL_INIT_FIELD(team_report_archive_dir,DFLT_G_TEAM_REPORT_ARCHIVE_DIR,archive_dir);
    GLOBAL_INIT_FIELD(team_extra_dir, DFLT_G_TEAM_EXTRA_DIR, var_dir);

    GLOBAL_INIT_FIELD(status_dir, DFLT_G_STATUS_DIR, var_dir);
    GLOBAL_INIT_FIELD(serve_socket, DFLT_G_SERVE_SOCKET, var_dir);
    if (global->variant_map_file) {
      GLOBAL_INIT_FIELD(variant_map_file, "", conf_dir);
    }
  }

  if (mode == PREPARE_COMPILE || mode == PREPARE_SERVE) {
    GLOBAL_INIT_FIELD(compile_dir, DFLT_G_COMPILE_DIR, var_dir);
    pathmake(global->compile_queue_dir, global->compile_dir, "/",
             DFLT_G_COMPILE_QUEUE_DIR, 0);
    info("global.compile_queue_dir is %s", global->compile_queue_dir);
    pathmake(global->compile_src_dir, global->compile_dir, "/",
             DFLT_G_COMPILE_SRC_DIR, 0);
    info("global.compile_src_dir is %s", global->compile_src_dir);
  }

  if (mode == PREPARE_SERVE) {
    pathmake(global->compile_out_dir, global->var_dir, "/",
             DFLT_G_COMPILE_DIR, 0);
    info("global.compile_out_dir is %s", global->compile_out_dir);
    pathmake(global->compile_status_dir, global->compile_out_dir, "/",
             DFLT_G_COMPILE_STATUS_DIR, 0);
    info("global.compile_status_dir is %s", global->compile_status_dir);
    pathmake(global->compile_report_dir, global->compile_out_dir, "/",
             DFLT_G_COMPILE_REPORT_DIR, 0);
    info("global.compile_report_dir is %s", global->compile_report_dir);
  }

  GLOBAL_INIT_FIELD(work_dir, DFLT_G_WORK_DIR, var_dir);
  GLOBAL_INIT_FIELD(print_work_dir, DFLT_G_PRINT_WORK_DIR, work_dir);
  GLOBAL_INIT_FIELD(diff_work_dir, DFLT_G_DIFF_WORK_DIR, work_dir);

  if (!global->a2ps_path[0]) {
    strcpy(global->a2ps_path, DFLT_G_A2PS_PATH);
  }
  if (!global->lpr_path[0]) {
    strcpy(global->lpr_path, DFLT_G_LPR_PATH);
  }
  if (!global->diff_path[0]) {
    strcpy(global->diff_path, DFLT_G_DIFF_PATH);
  }

  if (global->team_page_quota < 0) {
    global->team_page_quota = DFLT_G_TEAM_PAGE_QUOTA;
  }

  if (mode == PREPARE_COMPILE) {
    GLOBAL_INIT_FIELD(compile_work_dir, DFLT_G_COMPILE_WORK_DIR, work_dir);
  }

  if (mode == PREPARE_RUN || mode == PREPARE_SERVE) {
    GLOBAL_INIT_FIELD(run_dir, DFLT_G_RUN_DIR, var_dir);
    pathmake(global->run_queue_dir, global->run_dir, "/",
             DFLT_G_RUN_QUEUE_DIR, 0);
    info("global.run_queue_dir is %s", global->run_queue_dir);
    pathmake(global->run_exe_dir, global->run_dir, "/",
             DFLT_G_RUN_EXE_DIR, 0);
    info("global.run_exe_dir is %s", global->run_exe_dir);
  }
  if (mode == PREPARE_SERVE) {
    pathmake(global->run_out_dir, global->var_dir, "/", DFLT_G_RUN_DIR, 0);
    info("global.run_out_dir is %s", global->run_out_dir);
    pathmake(global->run_status_dir, global->run_out_dir, "/",
             DFLT_G_RUN_STATUS_DIR, 0);
    info("global.run_status_dir is %s", global->run_status_dir);
    pathmake(global->run_report_dir, global->run_out_dir, "/",
             DFLT_G_RUN_REPORT_DIR, 0);
    info("global.run_report_dir is %s", global->run_report_dir);
    if (global->team_enable_rep_view) {
      pathmake(global->run_team_report_dir, global->run_out_dir, "/",
               DFLT_G_RUN_TEAM_REPORT_DIR, 0);
      info("global.run_team_report_dir is %s", global->run_team_report_dir);
    }
  }

  if (mode == PREPARE_RUN) {
    GLOBAL_INIT_FIELD(run_work_dir, DFLT_G_RUN_WORK_DIR, work_dir);
    GLOBAL_INIT_FIELD(run_check_dir, DFLT_G_RUN_CHECK_DIR, work_dir);
  }

  /* score_system must be either "acm", either "kirov"
   * "acm" is the default
   */
  if (!global->score_system[0]) {
    global->score_system_val = SCORE_ACM;
  } else if (!strcmp(global->score_system, "acm")) {
    global->score_system_val = SCORE_ACM;
  } else if (!strcmp(global->score_system, "kirov")) {
    global->score_system_val = SCORE_KIROV;
  } else if (!strcmp(global->score_system, "olympiad")) {
    global->score_system_val = SCORE_OLYMPIAD;
  } else {
    err("Invalid scoring system: %s", global->score_system);
    return -1;
  }

  /*
   * Seconds rounding mode.
   */
  if (!global->rounding_mode[0]) {
    global->rounding_mode_val = SEC_CEIL;
  } else if (!strcmp(global->rounding_mode, "ceil")) {
    global->rounding_mode_val = SEC_CEIL;
  } else if (!strcmp(global->rounding_mode, "floor")) {
    global->rounding_mode_val = SEC_FLOOR;
  } else if (!strcmp(global->rounding_mode, "round")) {
    global->rounding_mode_val = SEC_ROUND;
  } else {
    err("Invalid rounding mode: %s", global->rounding_mode);
    return -1;
  }

  if (global->tests_to_accept == -1) {
    global->tests_to_accept = 1;
  }

  if (mode == PREPARE_COMPILE) {
    if (global->compile_real_time_limit == -1) {
      global->compile_real_time_limit = DFLT_G_COMPILE_REAL_TIME_LIMIT;
      info("global.compile_real_time_limit set to %d",
           global->compile_real_time_limit);
    }
  }

  if (mode == PREPARE_RUN) {
    if (global->checker_real_time_limit == -1) {
      global->checker_real_time_limit = DFLT_G_CHECKER_REAL_TIME_LIMIT;
      info("global.checker_real_time_limit set to %d",
           global->checker_real_time_limit);
    }
  }

  if (mode == PREPARE_SERVE) {
    if (!global->charset[0]) {
      snprintf(global->charset, sizeof(global->charset),
               "%s", DFLT_G_CHARSET);
      info("global.charset set to %s", global->charset);
    }
    /*
    if (!(global->charset_ptr = nls_lookup_table(global->charset))) {
      err("global.charset `%s' is invalid", global->charset);
      return -1;
    }
    */
    /*
    if (!global->standings_charset[0]) {
      snprintf(global->standings_charset, sizeof(global->standings_charset),
               "%s", global->charset);
      info("global.standings_charset set to %s", global->standings_charset);
    }
    if (!(global->standings_charset_ptr = nls_lookup_table(global->standings_charset))) {
      err("global.standings_charset `%s' is invalid", global->standings_charset);
      return -1;
    }
    */
    if (!global->standings_file_name[0]) {
      snprintf(global->standings_file_name,sizeof(global->standings_file_name),
               "%s", DFLT_G_STANDINGS_FILE_NAME);
      info("global.standings_file_name set to %s", global->standings_file_name);
    }

    if (global->contest_start_cmd[0]) {
      pathmake2(global->contest_start_cmd, global->conf_dir, "/",
                global->contest_start_cmd, 0);
      if (check_executable(global->contest_start_cmd) < 0) {
        err("contest start command %s is not executable or does not exist",
            global->contest_start_cmd);
        return -1;
      }
    }

    if (global->stand_header_file[0]) {
      pathmake2(global->stand_header_file, global->conf_dir, "/",
                global->stand_header_file, 0);
      r = generic_read_file((char**) &global->stand_header_txt, 0, &tmp_len, 0,
                            0, global->stand_header_file, "");
      if (r < 0) return -1;
    }

    if (global->stand_footer_file[0]) {
      pathmake2(global->stand_footer_file, global->conf_dir, "/",
                global->stand_footer_file, 0);
      r = generic_read_file((char**) &global->stand_footer_txt, 0, &tmp_len, 0,
                            0, global->stand_footer_file, "");
      if (r < 0) return -1;
    }

    if (global->stand2_file_name[0]) {
      if (global->stand2_header_file[0]) {
        pathmake2(global->stand2_header_file, global->conf_dir, "/",
                  global->stand2_header_file, 0);
        r = generic_read_file((char**) &global->stand2_header_txt, 0, &tmp_len,
                              0, 0, global->stand2_header_file, "");
        if (r < 0) return -1;
      }
      if (global->stand2_footer_file[0]) {
        pathmake2(global->stand2_footer_file, global->conf_dir, "/",
                  global->stand2_footer_file, 0);
        r = generic_read_file((char**) &global->stand2_footer_txt, 0, &tmp_len,
                              0, 0, global->stand2_footer_file, "");
        if (r < 0) return -1;
      } 
    }

    if (global->plog_file_name[0]) {
      if (global->plog_header_file[0]) {
        pathmake2(global->plog_header_file, global->conf_dir, "/",
                  global->plog_header_file, 0);
        r = generic_read_file((char**) &global->plog_header_txt, 0, &tmp_len,
                              0, 0, global->plog_header_file, "");
        if (r < 0) return -1;
      }
      if (global->plog_footer_file[0]) {
        pathmake2(global->plog_footer_file, global->conf_dir, "/",
                  global->plog_footer_file, 0);
        r = generic_read_file((char**) &global->plog_footer_txt, 0, &tmp_len,
                              0, 0, global->plog_footer_file, "");
        if (r < 0) return -1;
      }
      if (!global->plog_update_time) {
        global->plog_update_time = 30;
      }
    } else {
      global->plog_update_time = 0;
    }

    if (global->use_gzip < 0 || global->use_gzip > 1) {
      global->use_gzip = DFLT_G_USE_GZIP;
    }
    if (global->use_dir_hierarchy < 0 || global->use_dir_hierarchy > 1) {
      global->use_dir_hierarchy = DFLT_G_USE_DIR_HIERARCHY;
    }
    if (global->min_gzip_size < 0) {
      global->min_gzip_size = DFLT_G_MIN_GZIP_SIZE;
    }
  }

#if CONF_HAS_LIBINTL - 0 == 1
  if (global->enable_l10n < 0) global->enable_l10n = 1;
#if defined EJUDGE_LOCALE_DIR
  if (global->enable_l10n && !global->l10n_dir[0]) {
    strcpy(global->l10n_dir, EJUDGE_LOCALE_DIR);
  }
#endif /* EJUDGE_LOCALE_DIR */
  if (global->enable_l10n && !global->l10n_dir[0]) {
    global->enable_l10n = 0;
  }
#else
  global->enable_l10n = 0;
#endif /* CONF_HAS_LIBINTL */

#if CONF_HAS_LIBINTL - 0 == 1
  if (mode == PREPARE_SERVE && global->enable_l10n) {
    /* convert locale string into locale id */
    if (!strcmp(global->standings_locale, "ru_RU.KOI8-R")
        || !strcmp(global->standings_locale, "ru")) {
      global->standings_locale_id = 1;
    } else {
      global->standings_locale_id = 0;
    }
    info("standings_locale_id is %d", global->standings_locale_id);
  }
#endif /* CONF_HAS_LIBINTL */

  if (global->team_download_time == -1) {
    global->team_download_time = DFLT_G_TEAM_DOWNLOAD_TIME;
  }
  global->team_download_time *= 60;

  if (global->ignore_duplicated_runs == -1) {
    global->ignore_duplicated_runs = 1;
  }

  /* only run needs these parameters */
  if (mode == PREPARE_RUN) {
    if (!global->max_file_length) {
      global->max_file_length = DFLT_G_MAX_FILE_LENGTH;
      info("global.max_file_length set to %d", global->max_file_length);
    }
    if (!global->max_line_length) {
      global->max_line_length = DFLT_G_MAX_LINE_LENGTH;
      info("global.max_line_length set to %d", global->max_line_length);
    }
    if (!global->max_cmd_length) {
      global->max_cmd_length = DFLT_G_MAX_CMD_LENGTH;
      info("global.max_cmd_length set to %d", global->max_cmd_length);
    }

    if (global->sound_player[0]) {
      char *tmps;

      tmps = varsubst_heap(global->sound_player, 0,
                           section_global_params, section_problem_params,
                           section_language_params, section_tester_params);
      if (tmps != global->sound_player) {
        snprintf(global->sound_player, sizeof(global->sound_player),"%s",tmps);
        xfree(tmps);
      }
    }
  }

  if (mode == PREPARE_SERVE && global->user_priority_adjustments) {
    global->user_adjustment_info = parse_user_adjustment(global->user_priority_adjustments);
    if (!global->user_adjustment_info) return -1;
  }

  if (mode == PREPARE_SERVE) {
    if (global->contestant_status_num<0 || global->contestant_status_num>100) {
      err("global.contestant_status_num is invalid");
      return -1;
    }
  }
  if (mode == PREPARE_SERVE && global->contestant_status_num > 0) {
    // there must be exact number of legend entries
    if (!global->contestant_status_legend) {
      err("global.contestant_status_legend is not set");
      return -1;
    }
    for (i = 0; global->contestant_status_legend[i]; i++);
    if (i != global->contestant_status_num) {
      err("global.contestant_status_legend has different number of entries, than global.contestant_status_num");
      return -1;
    }
    if (global->contestant_status_row_attr) {
      for (i = 0; global->contestant_status_row_attr[i]; i++);
      if (i != global->contestant_status_num) {
        err("global.contestant_status_row_attr has different number of entries, than global.contestant_status_num");
        return -1;
      }
    }
  }

  for (i = 1; i <= max_lang && mode != PREPARE_RUN; i++) {
    if (!langs[i]) continue;
    if (!langs[i]->short_name[0]) {
      info("language.%d.short_name set to \"lang%d\"", i, i);
      sprintf(langs[i]->short_name, "lang%d", i);
    }
    if (!langs[i]->long_name[0]) {
      info("language.%d.long_name set to \"Language %d\"", i, i);
      sprintf(langs[i]->long_name, "Language %d", i);
    }
    
    if (mode == PREPARE_SERVE) {
      if (!langs[i]->compile_dir[0]) {
        pathcpy(langs[i]->compile_dir, global->compile_dir);
        info("language.%d.compile_dir is inherited from global ('%s')",
             i, langs[i]->compile_dir);
      }
      ASSERT(langs[i]->compile_dir);
      pathmake(langs[i]->compile_queue_dir, langs[i]->compile_dir, "/",
               DFLT_G_COMPILE_QUEUE_DIR, 0);
      info("language.%d.compile_queue_dir is %s",
           i, langs[i]->compile_queue_dir);
      pathmake(langs[i]->compile_src_dir, langs[i]->compile_dir, "/",
               DFLT_G_COMPILE_SRC_DIR, 0);
      info("language.%d.compile_src_dir is %s",
           i, langs[i]->compile_src_dir);
      snprintf(langs[i]->compile_out_dir, sizeof(langs[i]->compile_out_dir),
               "%s/%04d", langs[i]->compile_dir, global->contest_id);
      info("language.%d.compile_out_dir is %s",
           i, langs[i]->compile_out_dir);
    }

    if (!langs[i]->src_sfx[0]) {
      err("language.%d.src_sfx must be set", i);
      return -1;
    }

    if (mode == PREPARE_COMPILE) {
      if (!langs[i]->cmd[0]) {
        err("language.%d.cmd must be set", i);
        return -1;
      }
      pathmake4(langs[i]->cmd,global->script_dir, "/", langs[i]->cmd, NULL);
      info("language.%d.cmd is %s", i, langs[i]->cmd);
      if (langs[i]->compile_real_time_limit == -1) {
        langs[i]->compile_real_time_limit = global->compile_real_time_limit;
        info("language.%d.compile_real_time_limit is inherited from global (%d)", i, langs[i]->compile_real_time_limit);
      }
      ASSERT(langs[i]->compile_real_time_limit >= 0);
    }

    if (langs[i]->compiler_env) {
      for (j = 0; langs[i]->compiler_env[j]; j++) {
        langs[i]->compiler_env[j] = varsubst_heap(langs[i]->compiler_env[j], 1,
                                                  section_global_params,
                                                  section_problem_params,
                                                  section_language_params,
                                                  section_tester_params);
        if (!langs[i]->compiler_env[j]) return -1;
      }
    }
  }

  for (i = 0; i < max_abstr_prob && mode != PREPARE_COMPILE; i++) {
    if (!abstr_probs[i]->short_name[0]) {
      err("abstract problem must define problem short name");
      return -1;
    }
    ish = abstr_probs[i]->short_name;
    if (abstr_probs[i]->id) {
      err("abstract problem %s must not define problem id", ish);
      return -1;
    }
    if (abstr_probs[i]->long_name[0]) {
      err("abstract problem %s must not define problem long name", ish);
      return -1;
    }
    if (abstr_probs[i]->super[0]) {
      err("abstract problem %s cannot have a superproblem", ish);
      return -1;
    }
  }

  for (i = 1; i <= max_prob && mode != PREPARE_COMPILE; i++) {
    if (!probs[i]) continue;
    si = -1;
    sish = 0;
    if (probs[i]->super[0]) {
      for (si = 0; si < max_abstr_prob; si++)
        if (!strcmp(abstr_probs[si]->short_name, probs[i]->super))
          break;
      if (si >= max_abstr_prob) {
        err("abstract problem `%s' is not defined", probs[i]->super);
        return -1;
      }
      sish = abstr_probs[si]->short_name;
    }
    if (!probs[i]->short_name[0] && global->auto_short_problem_name) {
      snprintf(probs[i]->short_name, sizeof(probs[i]->short_name),
               "%05d", probs[i]->id);
      info("problem %d short name is set to %s", i, probs[i]->short_name);
    }
    if (!probs[i]->short_name[0]) {
      err("problem %d short name must be set", i);
      return -1;
    }
    ish = probs[i]->short_name;
    if (!probs[i]->long_name[0]) {
      info("problem.%s.long_name set to \"Problem %s\"", ish, ish);
      sprintf(probs[i]->long_name, "Problem %s", ish);
    }

    if (probs[i]->team_enable_rep_view == -1 && si != -1
        && abstr_probs[si]->team_enable_rep_view != -1) {
      probs[i]->team_enable_rep_view = abstr_probs[si]->team_enable_rep_view;
      info("problem.%s.team_enable_rep_view inherited from problem.%s (%d)",
           ish, sish, probs[i]->team_enable_rep_view);
    }
    if (probs[i]->team_enable_rep_view == -1) {
      info("problem.%s.team_enable_rep_view inherited from global (%d)",
           ish, global->team_enable_rep_view);
      probs[i]->team_enable_rep_view = global->team_enable_rep_view;
    } else if (probs[i]->team_enable_rep_view == -1) {
      probs[i]->team_enable_rep_view = 0;
    }

    if (probs[i]->team_enable_ce_view == -1 && si != -1
        && abstr_probs[si]->team_enable_ce_view != -1) {
      probs[i]->team_enable_ce_view = abstr_probs[si]->team_enable_ce_view;
      info("problem.%s.team_enable_ce_view inherited from problem.%s (%d)",
           ish, sish, probs[i]->team_enable_ce_view);
    }
    if (probs[i]->team_enable_ce_view == -1) {
      info("problem.%s.team_enable_ce_view inherited from global (%d)",
           ish, global->team_enable_ce_view);
      probs[i]->team_enable_ce_view = global->team_enable_ce_view;
    } else if (probs[i]->team_enable_ce_view == -1) {
      probs[i]->team_enable_ce_view = 0;
    }

    if (probs[i]->team_show_judge_report == -1 && si != -1
        && abstr_probs[si]->team_show_judge_report != -1) {
      probs[i]->team_show_judge_report=abstr_probs[si]->team_show_judge_report;
      info("problem.%s.team_show_judge_report inherited from problem.%s (%d)",
           ish, sish, probs[i]->team_show_judge_report);
    }
    if (probs[i]->team_show_judge_report == -1) {
      info("problem.%s.team_show_judge_report inherited from global (%d)",
           ish, global->team_show_judge_report);
      probs[i]->team_show_judge_report = global->team_show_judge_report;
    } else if (probs[i]->team_show_judge_report == -1) {
      probs[i]->team_show_judge_report = 0;
    }

    if (probs[i]->tests_to_accept == -1 && si != -1
        && abstr_probs[si]->tests_to_accept != -1) {
      probs[i]->tests_to_accept = abstr_probs[si]->tests_to_accept;
      info("problem.%s.tests_to_accept inherited from problem.%s (%d)",
           ish, sish, probs[i]->tests_to_accept);
    }
    if (probs[i]->tests_to_accept == -1) {
      probs[i]->tests_to_accept = global->tests_to_accept;
      info("problem.%s.tests_to_accept inherited from global (%d)",
           ish, global->tests_to_accept);
    }
    if (probs[i]->tests_to_accept == -1) {
      probs[i]->tests_to_accept = 0;
    }

    if (probs[i]->accept_partial == -1 && si != -1
        && abstr_probs[si]->accept_partial != -1) {
      probs[i]->accept_partial = abstr_probs[si]->accept_partial;
      info("problem.%s.accept_partial inherited from problem.%s (%d)",
           ish, sish, probs[i]->accept_partial);
    }
    if (probs[i]->accept_partial == -1) {
      probs[i]->accept_partial = 0;
    }

    if (probs[i]->disable_auto_testing == -1 && si != -1
        && abstr_probs[si]->disable_auto_testing != -1) {
      probs[i]->disable_auto_testing = abstr_probs[si]->disable_auto_testing;
      info("problem.%s.disable_auto_testing inherited from problem.%s (%d)",
           ish, sish, probs[i]->disable_auto_testing);
    }
    if (probs[i]->disable_auto_testing == -1) {
      probs[i]->disable_auto_testing = global->disable_auto_testing;
      info("problem.%s.disable_auto_testing inherited from global (%d)",
           ish, global->disable_auto_testing);
    }
    if (probs[i]->disable_auto_testing == -1) {
      probs[i]->disable_auto_testing = 0;
    }

    if (probs[i]->disable_testing == -1 && si != -1
        && abstr_probs[si]->disable_testing != -1) {
      probs[i]->disable_testing = abstr_probs[si]->disable_testing;
      info("problem.%s.disable_testing inherited from problem.%s (%d)",
           ish, sish, probs[i]->disable_testing);
    }
    if (probs[i]->disable_testing == -1) {
      probs[i]->disable_testing = global->disable_testing;
      info("problem.%s.disable_testing inherited from global (%d)",
           ish, global->disable_testing);
    }
    if (probs[i]->disable_testing == -1) {
      probs[i]->disable_testing = 0;
    }

    if (!probs[i]->full_score && si != -1
        && abstr_probs[si]->full_score) {
      probs[i]->full_score = abstr_probs[si]->full_score;
      info("problem.%s.full_score inherited from problem.%s (%d)",
           ish, sish, probs[i]->full_score);
    }
    if (!probs[i]->full_score) {
      probs[i]->full_score = DFLT_P_FULL_SCORE;
      info("problem.%s.full_score set to %d", ish, DFLT_P_FULL_SCORE);
    }

    if (probs[i]->variable_full_score == -1 && si != -1
        && abstr_probs[si]->variable_full_score >= 0) {
      probs[i]->variable_full_score = abstr_probs[si]->variable_full_score;
      info("problem.%s.variable_full_score inherited from problem.%s (%d)",
           ish, sish, probs[i]->variable_full_score);
    }
    if (probs[i]->variable_full_score == -1) {
      probs[i]->variable_full_score = 0;
      info("problem.%s.variable_full_score set to %d", ish,
           DFLT_P_VARIABLE_FULL_SCORE);
    }

    if (probs[i]->hidden == -1 && si != -1 && abstr_probs[si]->hidden >= 0) {
      probs[i]->hidden = abstr_probs[si]->hidden;
      info("problem.%s.hidden inherited from problem.%s (%d)",
           ish, sish, probs[i]->hidden);
    }
    if (probs[i]->hidden == -1) {
      probs[i]->hidden = 0;
      info("problem.%s.hidden set to %d", ish, DFLT_P_HIDDEN);
    }

    if (probs[i]->test_score < 0 && si != -1
        && abstr_probs[si]->test_score >= 0) {
      probs[i]->test_score = abstr_probs[si]->test_score;
      info("problem.%s.test_score inherited from problem.%s (%d)",
           ish, sish, probs[i]->test_score);
    }
    if (probs[i]->test_score < 0) {
      probs[i]->test_score = DFLT_P_TEST_SCORE;
      info("problem.%s.test_score set to %d", ish,  DFLT_P_TEST_SCORE);
    }

    if (probs[i]->run_penalty == -1 && si != -1
        && abstr_probs[si]->run_penalty >= 0) {
      probs[i]->run_penalty = abstr_probs[si]->run_penalty;
      info("problem.%s.run_penalty inherited from problem.%s (%d)",
           ish, sish, probs[i]->run_penalty);
    }
    if (probs[i]->run_penalty == -1) {
      probs[i]->run_penalty = DFLT_P_RUN_PENALTY;
      info("problem.%s.run_penalty set to %d", ish, DFLT_P_RUN_PENALTY);
    }

    if (probs[i]->disqualified_penalty == -1 && si != -1
        && abstr_probs[si]->disqualified_penalty >= 0) {
      probs[i]->disqualified_penalty = abstr_probs[si]->disqualified_penalty;
      info("problem.%s.disqualified_penalty inherited from problem.%s (%d)",
           ish, sish, probs[i]->disqualified_penalty);
    }
    if (probs[i]->disqualified_penalty == -1) {
      probs[i]->disqualified_penalty = probs[i]->run_penalty;
      info("problem.%s.disqualified_penalty set to %d", ish, 
           probs[i]->run_penalty);
    }
    
    if (probs[i]->use_stdin == -1 && si != -1
        && abstr_probs[si]->use_stdin != -1) {
      probs[i]->use_stdin = abstr_probs[si]->use_stdin;
      info("problem.%s.use_stdin inherited from problem.%s (%d)",
           ish, sish, probs[i]->use_stdin);
    }
    if (probs[i]->use_stdin == -1) {
      probs[i]->use_stdin = 0;
      info("problem.%s.use_stdin set to %d", ish, 0);
    }

    if (probs[i]->use_stdout == -1 && si != -1
        && abstr_probs[si]->use_stdout != -1) {
      probs[i]->use_stdout = abstr_probs[si]->use_stdout;
      info("problem.%s.use_stdout inherited from problem.%s (%d)",
           ish, sish, probs[i]->use_stdout);
    }
    if (probs[i]->use_stdout == -1) {
      probs[i]->use_stdout = 0;
      info("problem.%s.use_stdout set to %d", ish, 0);
    }

    if (!probs[i]->time_limit && si != -1 && abstr_probs[si]->time_limit) {
      probs[i]->time_limit = abstr_probs[si]->time_limit;
      info("problem.%s.time_limit inherited from problem.%s (%d)",
           ish, sish, probs[i]->time_limit);
    }
    if (!probs[i]->real_time_limit && si != -1
        && abstr_probs[si]->real_time_limit) {
      probs[i]->real_time_limit = abstr_probs[si]->real_time_limit;
      info("problem.%s.real_time_limit inherited from problem.%s (%d)",
           ish, sish, probs[i]->real_time_limit);
    }
    /*
    if (!probs[i]->test_score_list[0] && si != -1
        && abstr_probs[si]->test_score_list[0]) {
      strcpy(probs[i]->test_score_list, abstr_probs[si]->test_score_list);
      info("problem.%s.test_score_list inherited from problem.%s (`%s')",
           ish, sish, probs[i]->test_score_list);
    }
    */
    if (probs[i]->test_sfx[0] == 1 && si != -1 &&
        abstr_probs[si]->test_sfx[0] != 1) {
      strcpy(probs[i]->test_sfx, abstr_probs[si]->test_sfx);
      info("problem.%s.test_sfx inherited from problem.%s ('%s')",
           ish, sish, probs[i]->test_sfx);
    }
    if (probs[i]->test_sfx[0] == 1 && global->test_sfx[0] != 1) {
      strcpy(probs[i]->test_sfx, global->test_sfx);
      info("problem.%s.test_sfx inherited from global ('%s')",
           ish, probs[i]->test_sfx);
    }
    if (probs[i]->test_sfx[0] == 1) {
      probs[i]->test_sfx[0] = 0;
    }
    if (probs[i]->corr_sfx[0] == 1 && si != -1 &&
        abstr_probs[si]->corr_sfx[0] != 1) {
      strcpy(probs[i]->corr_sfx, abstr_probs[si]->corr_sfx);
      info("problem.%s.corr_sfx inherited from problem.%s ('%s')",
           ish, sish, probs[i]->corr_sfx);
    }
    if (probs[i]->corr_sfx[0] == 1 && global->corr_sfx[0] != 1) {
      strcpy(probs[i]->corr_sfx, global->corr_sfx);
      info("problem.%s.corr_sfx inherited from global ('%s')",
           ish, probs[i]->corr_sfx);
    }
    if (probs[i]->corr_sfx[0] == 1) {
      probs[i]->corr_sfx[0] = 0;
    }

    if (probs[i]->info_sfx[0] == 1 && si != -1 &&
        abstr_probs[si]->info_sfx[0] != 1) {
      strcpy(probs[i]->info_sfx, abstr_probs[si]->info_sfx);
      info("problem.%s.info_sfx inherited from problem.%s ('%s')",
           ish, sish, probs[i]->info_sfx);
    }
    if (probs[i]->info_sfx[0] == 1 && global->info_sfx[0]) {
      strcpy(probs[i]->info_sfx, global->info_sfx);
      info("problem.%s.info_sfx inherited from global ('%s')",
           ish, probs[i]->info_sfx);
    }
    if (probs[i]->info_sfx[0] == 1) {
      probs[i]->info_sfx[0] = 0;
    }

    if (probs[i]->tgz_sfx[0] == 1 && si != -1 &&
        abstr_probs[si]->tgz_sfx[0] != 1) {
      strcpy(probs[i]->tgz_sfx, abstr_probs[si]->tgz_sfx);
      info("problem.%s.tgz_sfx inherited from problem.%s ('%s')",
           ish, sish, probs[i]->tgz_sfx);
    }
    if (probs[i]->tgz_sfx[0] == 1 && global->tgz_sfx[0]) {
      strcpy(probs[i]->tgz_sfx, global->tgz_sfx);
      info("problem.%s.tgz_sfx inherited from global ('%s')",
           ish, probs[i]->tgz_sfx);
    }
    if (probs[i]->tgz_sfx[0] == 1) {
      probs[i]->tgz_sfx[0] = 0;
    }

    // set up test pattern
    if (probs[i]->test_pat[0] == 1 && si != -1 &&
        abstr_probs[si]->test_pat[0] != 1) {
      strcpy(probs[i]->test_pat, abstr_probs[si]->test_pat);
      info("problem.%s.test_pat inherited from problem.%s ('%s')",
           ish, sish, probs[i]->test_pat);
    }
    if (probs[i]->test_pat[0] == 1 && global->test_pat[0] != 1) {
      strcpy(probs[i]->test_pat, global->test_pat);
      info("problem.%s.test_pat inherited from global ('%s')",
           ish, probs[i]->test_pat);
    }
    if (probs[i]->test_pat[0] == 1) {
      probs[i]->test_pat[0] = 0;
    }

    if (probs[i]->corr_pat[0] == 1 && si != -1 &&
        abstr_probs[si]->corr_pat[0] != 1) {
      strcpy(probs[i]->corr_pat, abstr_probs[si]->corr_pat);
      info("problem.%s.corr_pat inherited from problem.%s ('%s')",
           ish, sish, probs[i]->corr_pat);
    }
    if (probs[i]->corr_pat[0] == 1 && global->corr_pat[0] != 1) {
      strcpy(probs[i]->corr_pat, global->corr_pat);
      info("problem.%s.corr_pat inherited from global ('%s')",
           ish, probs[i]->corr_pat);
    }
    if (probs[i]->corr_pat[0] == 1) {
      probs[i]->corr_pat[0] = 0;
    }

    if (probs[i]->info_pat[0] == 1 && si != -1 &&
        abstr_probs[si]->info_pat[0] != 1) {
      strcpy(probs[i]->info_pat, abstr_probs[si]->info_pat);
      info("problem.%s.info_pat inherited from problem.%s ('%s')",
           ish, sish, probs[i]->info_pat);
    }
    if (probs[i]->info_pat[0] == 1 && global->info_pat[0]) {
      strcpy(probs[i]->info_pat, global->info_pat);
      info("problem.%s.info_pat inherited from global ('%s')",
           ish, probs[i]->info_pat);
    }
    if (probs[i]->info_pat[0] == 1) {
      probs[i]->info_pat[0] = 0;
    }

    if (probs[i]->tgz_pat[0] == 1 && si != -1 &&
        abstr_probs[si]->tgz_pat[0] != 1) {
      strcpy(probs[i]->tgz_pat, abstr_probs[si]->tgz_pat);
      info("problem.%s.tgz_pat inherited from problem.%s ('%s')",
           ish, sish, probs[i]->tgz_pat);
    }
    if (probs[i]->tgz_pat[0] == 1 && global->tgz_pat[0]) {
      strcpy(probs[i]->tgz_pat, global->tgz_pat);
      info("problem.%s.tgz_pat inherited from global ('%s')",
           ish, probs[i]->tgz_pat);
    }
    if (probs[i]->tgz_pat[0] == 1) {
      probs[i]->tgz_pat[0] = 0;
    }

    if (probs[i]->priority_adjustment == -1000 && si != -1 &&
        abstr_probs[si]->priority_adjustment != -1000) {
      probs[i]->priority_adjustment = abstr_probs[si]->priority_adjustment;
    }
    if (probs[i]->priority_adjustment == -1000) {
      probs[i]->priority_adjustment = 0;
    }

    if (!probs[i]->score_multiplier && si != -1 &&
        abstr_probs[si]->score_multiplier >= 1) {
      probs[i]->score_multiplier = abstr_probs[si]->score_multiplier;
    }

    if (mode == PREPARE_SERVE) {
      if (probs[i]->deadline[0]) {
        if (parse_date(probs[i]->deadline, &probs[i]->t_deadline) < 0) {
          err("invalid deadline specified for problem `%s'",
              probs[i]->short_name);
          return -1;
        }
        info("problem.%s.deadline is %ld", ish, probs[i]->t_deadline);
      }
      if (probs[i]->personal_deadline) {
        if (parse_personal_deadlines(probs[i]->personal_deadline,
                                     &probs[i]->pd_total,
                                     &probs[i]->pd_infos) < 0) {
          return -1;
        }
      }
      if (probs[i]->start_date[0]) {
        if (parse_date(probs[i]->start_date, &probs[i]->t_start_date) < 0) {
          err("invalid start_date specified for problem `%s'",
              probs[i]->short_name);
          return -1;
        }
        info("problem.%s.start_date is %ld", ish, probs[i]->t_start_date);
      }

      if (parse_deadline_penalties(probs[i]->date_penalty,
                                   &probs[i]->dp_total,
                                   &probs[i]->dp_infos) < 0) return -1;

      if (si != -1 && abstr_probs[si]->disable_language) {
        probs[i]->disable_language = sarray_merge_pf(abstr_probs[si]->disable_language, probs[i]->disable_language);
      }
      if (si != -1 && abstr_probs[si]->checker_env) {
        probs[i]->checker_env = sarray_merge_pf(abstr_probs[si]->checker_env,
                                                probs[i]->checker_env);
      }
      if (probs[i]->checker_env) {
        for (j = 0; probs[i]->checker_env[j]; j++) {
          probs[i]->checker_env[j] = varsubst_heap(probs[i]->checker_env[j], 1,
                                                   section_global_params,
                                                   section_problem_params,
                                                   section_language_params,
                                                   section_tester_params);
          if (!probs[i]->checker_env[j]) return -1;
        }
      }
    }

    if (mode == PREPARE_RUN) {
      if (!probs[i]->test_dir[0] && si != -1
          && abstr_probs[si]->test_dir[0]) {
        sformat_message(probs[i]->test_dir, PATH_MAX,
                        abstr_probs[si]->test_dir,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.test_dir taken from problem.%s ('%s')",
             ish, sish, probs[i]->test_dir);
      }
      if (!probs[i]->test_dir[0]) {
        info("problem.%s.test_dir set to %s", ish, probs[i]->short_name);
        pathcpy(probs[i]->test_dir, probs[i]->short_name);
      }
      path_add_dir(probs[i]->test_dir, global->test_dir);
      info("problem.%s.test_dir is '%s'", 
           ish, probs[i]->test_dir);

      if (!probs[i]->corr_dir[0] && si != -1
          && abstr_probs[si]->corr_dir[0]) {
        sformat_message(probs[i]->corr_dir, PATH_MAX,
                        abstr_probs[si]->corr_dir,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.corr_dir taken from problem.%s ('%s')",
             ish, sish, probs[i]->corr_dir);
      }
      if (probs[i]->corr_dir[0]) {
        path_add_dir(probs[i]->corr_dir, global->corr_dir);
        info("problem.%s.corr_dir is '%s'", ish, probs[i]->corr_dir);
      }

      if (probs[i]->use_info == -1 && si != -1
          && abstr_probs[si]->use_info != -1) {
        probs[i]->use_info = abstr_probs[si]->use_info;
        info("problem.%s.use_info taken from problem.%s (%d)",
             ish, sish, probs[i]->use_info);
      }
      if (probs[i]->use_info == -1) {
        probs[i]->use_info = 0;
      }

      if (!probs[i]->info_dir[0] && si != -1 && probs[i]->use_info
          && abstr_probs[si]->info_dir[0]) {
        sformat_message(probs[i]->info_dir, PATH_MAX,
                        abstr_probs[si]->info_dir,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.info_dir taken from problem.%s ('%s')",
             ish, sish, probs[i]->info_dir);
      }
      if (!probs[i]->info_dir[0] && probs[i]->use_info) {
        pathcpy(probs[i]->info_dir, probs[i]->short_name);
        info("problem.%s.info_dir is set to '%s'", ish, probs[i]->info_dir);
      }
      if (probs[i]->use_info) {
        path_add_dir(probs[i]->info_dir, global->info_dir);
        info("problem.%s.info_dir is '%s'", ish, probs[i]->info_dir);
      }

      if (probs[i]->use_tgz == -1 && si != -1
          && abstr_probs[si]->use_tgz != -1) {
        probs[i]->use_tgz = abstr_probs[si]->use_tgz;
        info("problem.%s.use_tgz taken from problem.%s (%d)",
             ish, sish, probs[i]->use_tgz);
      }
      if (probs[i]->use_tgz == -1) {
        probs[i]->use_tgz = 0;
      }

      if (!probs[i]->tgz_dir[0] && si != -1 && probs[i]->use_tgz
          && abstr_probs[si]->tgz_dir[0]) {
        sformat_message(probs[i]->tgz_dir, PATH_MAX,
                        abstr_probs[si]->tgz_dir,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.tgz_dir taken from problem.%s ('%s')",
             ish, sish, probs[i]->tgz_dir);
      }
      if (!probs[i]->tgz_dir[0] && probs[i]->use_tgz) {
        pathcpy(probs[i]->tgz_dir, probs[i]->short_name);
        info("problem.%s.tgz_dir is set to '%s'", ish, probs[i]->tgz_dir);
      }
      if (probs[i]->use_tgz) {
        path_add_dir(probs[i]->tgz_dir, global->tgz_dir);
        info("problem.%s.tgz_dir is '%s'", ish, probs[i]->tgz_dir);
      }

      if (!probs[i]->input_file[0] && si != -1
          && abstr_probs[si]->input_file[0]) {
        sformat_message(probs[i]->input_file, PATH_MAX,
                        abstr_probs[si]->input_file,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.input_file inherited from problem.%s ('%s')",
             ish, sish, probs[i]->input_file);
      }
      if (!probs[i]->input_file[0]) {
        info("problem.%s.input_file set to %s", ish, DFLT_P_INPUT_FILE);
        snprintf(probs[i]->input_file, sizeof(probs[i]->input_file),
                 "%s", DFLT_P_INPUT_FILE);
      }
      if (!probs[i]->output_file[0] && si != -1
          && abstr_probs[si]->output_file[0]) {
        sformat_message(probs[i]->output_file, PATH_MAX,
                        abstr_probs[si]->output_file,
                        NULL, probs[i], NULL, NULL, NULL, 0, 0, 0);
        info("problem.%s.output_file inherited from problem.%s ('%s')",
             ish, sish, probs[i]->output_file);
      }
      if (!probs[i]->output_file[0]) {
        info("problem.%s.output_file set to %s", ish, DFLT_P_OUTPUT_FILE);
        snprintf(probs[i]->output_file, sizeof(probs[i]->output_file),
                 "%s", DFLT_P_OUTPUT_FILE);
      }

      if (probs[i]->variant_num == -1 && si != -1
          && abstr_probs[si]->variant_num != -1) {
        probs[i]->variant_num = abstr_probs[si]->variant_num;
        info("problem.%s.variant_num inherited from problem.%s (%d)",
             ish, sish, probs[i]->variant_num);
      }
      if (probs[i]->variant_num == -1) {
        probs[i]->variant_num = 0;
      }

      if (probs[i]->use_corr == -1 && si != -1
          && abstr_probs[si]->use_corr != -1) {
        probs[i]->use_corr = abstr_probs[si]->use_corr;
        info("problem.%s.use_corr inherited from problem.%s (%d)",
             ish, sish, probs[i]->use_corr);
      }
      if (probs[i]->use_corr == -1 && probs[i]->corr_dir[0]) {
        probs[i]->use_corr = 1;
      }
      if (probs[i]->use_corr == -1) {
        probs[i]->use_corr = 0;
      }
      if (probs[i]->checker_real_time_limit == -1 && si != -1
          && abstr_probs[si]->checker_real_time_limit != -1) {
        probs[i]->checker_real_time_limit = abstr_probs[si]->checker_real_time_limit;
        info("problem.%s.checker_real_time_limit inherited from problem.%s (%d)", ish, sish, probs[i]->checker_real_time_limit);
      }
      if (probs[i]->checker_real_time_limit == -1) {
        probs[i]->checker_real_time_limit = global->checker_real_time_limit;
        info("problem.%s.checker_real_time_limit inherited from global (%d)",
           ish, probs[i]->checker_real_time_limit);
      }
      ASSERT(probs[i]->checker_real_time_limit >= 0);
    }

    if (probs[i]->test_sets) {
      if (parse_testsets(probs[i]->test_sets,
                         &probs[i]->ts_total,
                         &probs[i]->ts_infos) < 0)
        return -1;
    }
  }

  if (mode == PREPARE_SERVE || mode == PREPARE_RUN) {
    for (i = 0; i < max_abstr_tester; i++) {
      if (process_abstract_tester(i) < 0) return -1;
    }
  }

  if (mode == PREPARE_SERVE) {
    int var_prob_num = 0;

    for (i = 0; i <= max_prob; i++)
      if (probs[i] && probs[i]->variant_num > 0) var_prob_num++;
    if (var_prob_num > 0) {
      if (!global->variant_map_file[0]) {
        err("There are variant problems, but no variant file name");
        return -1;
      }
      global->variant_map = parse_variant_map(global->variant_map_file);
      if (!global->variant_map) return -1;
    }
  }

#define TESTER_INIT_FIELD(f,d,c) do { if (!testers[i]->f[0]) { info("tester.%d.%s set to %s", i, #f, d); pathcat(testers[i]->f, d); } path_add_dir(testers[i]->f, testers[i]->c); } while(0)
  if (mode == PREPARE_SERVE || mode == PREPARE_RUN) {
    for (i = 1; i <= max_tester; i++) {
      struct section_tester_data *tp = 0;
      struct section_tester_data *atp = 0;

      if (!testers[i]) continue;
      tp = testers[i];

      /* we hardly can do any reasonable in this case */
      if (tp->any && mode == PREPARE_RUN) {
        continue;
      }

      si = -1;
      sish = 0;
      if (tp->super && tp->super[0]) {
        if (tp->super[1]) {
          err("concrete tester may inherit only one abstract tester");
          return -1;
        }
        for (si = 0; si < max_abstr_tester; si++) {
          atp = abstr_testers[si];
          if (!strcmp(atp->name, tp->super[0]))
            break;
        }
        if (si >= max_abstr_tester) {
          err("abstract tester %s not found", tp->super[0]);
          return -1;
        }
        sish = atp->name;
      }

      /* copy arch and key */
      if (!tp->arch[0] && atp && atp->arch[0]) {
        strcpy(tp->arch, atp->arch);
        info("tester.%d.arch inherited from tester.%s ('%s')",
             i, sish, tp->arch);
      }
      if (!tp->key[0] && atp && atp->key[0]) {
        strcpy(tp->key, atp->key);
        info("tester.%d.key inherited from tester.%s ('%s')",
             i, sish, tp->key);
      }

      if (!testers[i]->name[0]) {
        sprintf(testers[i]->name, "tst_%d", testers[i]->id);
        if (testers[i]->arch[0]) {
          sprintf(testers[i]->name + strlen(testers[i]->name),
                  "_%s", testers[i]->arch);
        }
        info("tester.%d.name set to \"%s\"", i, testers[i]->name);
      }

      if (mode == PREPARE_RUN) {
        if (!tp->check_dir[0] && atp && atp->check_dir[0]) {
          sformat_message(tp->check_dir, PATH_MAX, atp->check_dir,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.check_dir inherited from tester.%s ('%s')",
               i, sish, tp->check_dir);
        }
        if (!tp->check_dir[0]) {
          info("tester.%d.check_dir inherited from global ('%s')",
               i, global->run_check_dir);
          pathcpy(tp->check_dir, global->run_check_dir);
        }
      }

      if (mode == PREPARE_SERVE) {
        if (!tp->run_dir[0] && atp && atp->run_dir[0]) {
          sformat_message(tp->run_dir, PATH_MAX, atp->run_dir,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.run_dir inherited from tester.%s ('%s')",
               i, sish, tp->run_dir);
        }
        if (!tp->run_dir[0]) {
          info("tester.%d.run_dir inherited from global ('%s')",
               i, global->run_dir);
          pathcpy(tp->run_dir, global->run_dir);
        }
        pathmake(tp->run_queue_dir, tp->run_dir, "/",
                 DFLT_G_RUN_QUEUE_DIR, 0);
        info("tester.%d.run_queue_dir is %s", i, tp->run_queue_dir);
        pathmake(tp->run_exe_dir, tp->run_dir, "/",
                 DFLT_G_RUN_EXE_DIR, 0);
        info("tester.%d.run_exe_dir is %s", i, tp->run_exe_dir);
        snprintf(tp->run_out_dir, sizeof(tp->run_out_dir), "%s/%04d",
                 tp->run_dir, global->contest_id);
        info("tester.%d.run_out_dir is %s", i, tp->run_out_dir);

        if (tp->priority_adjustment == -1000 && atp
            && atp->priority_adjustment != -1000) {
          tp->priority_adjustment = atp->priority_adjustment;
        }
        if (tp->priority_adjustment == -1000) {
          tp->priority_adjustment = 0;
        }
      }

      if (tp->no_core_dump == -1 && atp && atp->no_core_dump != -1) {
        tp->no_core_dump = atp->no_core_dump;
        info("tester.%d.no_core_dump inherited from tester.%s (%d)",
             i, sish, tp->no_core_dump);        
      }
      if (tp->no_core_dump == -1) {
        tp->no_core_dump = 0;
      }
      if (tp->clear_env == -1 && atp && atp->clear_env != -1) {
        tp->clear_env = atp->clear_env;
        info("tester.%d.clear_env inherited from tester.%s (%d)",
             i, sish, tp->clear_env);
      }
      if (tp->clear_env == -1) {
        tp->clear_env = 0;
      }
      if (tp->time_limit_adjustment == -1
          && atp && atp->time_limit_adjustment != -1) {
        tp->time_limit_adjustment = atp->time_limit_adjustment;
        info("tester.%d.time_limit_adjustment inherited from tester.%s (%d)",
             i, sish, tp->time_limit_adjustment);
      }
      if (tp->time_limit_adjustment == -1) {
        tp->time_limit_adjustment = 0;
      }
      if (!tp->kill_signal[0] && atp && atp->kill_signal[0]) {
        strcpy(tp->kill_signal, atp->kill_signal);
        info("tester.%d.kill_signal inherited from tester.%s ('%s')",
             i, sish, tp->kill_signal);
      }
      if (!tp->max_stack_size && atp && atp->max_stack_size) {
        tp->max_stack_size = atp->max_stack_size;
        info("tester.%d.max_stack_size inherited from tester.%s (%d)",
             i, sish, tp->max_stack_size);        
      }
      if (!tp->max_data_size && atp && atp->max_data_size) {
        tp->max_data_size = atp->max_data_size;
        info("tester.%d.max_data_size inherited from tester.%s (%d)",
             i, sish, tp->max_data_size);        
      }
      if (!tp->max_vm_size && atp && atp->max_vm_size) {
        tp->max_vm_size = atp->max_vm_size;
        info("tester.%d.max_vm_size inherited from tester.%s (%d)",
             i, sish, tp->max_vm_size);        
      }

      if (tp->is_dos == -1 && atp && atp->is_dos != -1) {
        tp->is_dos = atp->is_dos;
        info("tester.%d.is_dos inherited from tester.%s (%d)",
             i, sish, tp->is_dos);        
      }
      if (tp->is_dos == -1) {
        tp->is_dos = 0;
      }
      if (tp->no_redirect == -1 && atp && atp->no_redirect != -1) {
        tp->no_redirect = atp->no_redirect;
        info("tester.%d.no_redirect inherited from tester.%s (%d)",
             i, sish, tp->no_redirect);        
      }
      if (tp->no_redirect == -1) {
        tp->no_redirect = 0;
      }
      if (!tp->errorcode_file[0] && atp && atp->errorcode_file) {
        sformat_message(tp->errorcode_file, PATH_MAX, atp->errorcode_file,
                        global, probs[tp->problem], NULL,
                        tp, NULL, 0, 0, 0);
        info("tester.%d.errorcode_file inherited from tester.%s ('%s')",
             i, sish, tp->errorcode_file);        
      }

      if (atp && atp->start_env) {
        tp->start_env = sarray_merge_pf(atp->start_env, tp->start_env);
      }
      if (tp->start_env) {
        for (j = 0; tp->start_env[j]; j++) {
          tp->start_env[j] = varsubst_heap(tp->start_env[j], 1,
                                           section_global_params,
                                           section_problem_params,
                                           section_language_params,
                                           section_tester_params);
          if (!tp->start_env[j]) return -1;
        }
      }
      if (atp && atp->checker_env) {
        tp->checker_env = sarray_merge_pf(atp->checker_env, tp->checker_env);
      }
      if (tp->checker_env) {
        for (j = 0; tp->checker_env[j]; j++) {
          tp->checker_env[j] = varsubst_heap(tp->checker_env[j], 1,
                                             section_global_params,
                                             section_problem_params,
                                             section_language_params,
                                             section_tester_params);
          if (!tp->checker_env[j]) return -1;
        }
      }

      if (mode == PREPARE_RUN) {
        if (!tp->error_file[0] && atp && atp->error_file[0]) {
          sformat_message(tp->error_file, PATH_MAX, atp->error_file,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.error_file inherited from tester.%s ('%s')",
               i, sish, tp->error_file);        
        }
        if (!testers[i]->error_file[0]) {
          info("tester.%d.error_file set to %s", i, DFLT_T_ERROR_FILE);
          snprintf(testers[i]->error_file, sizeof(testers[i]->error_file),
                   "%s", DFLT_T_ERROR_FILE);
        }
        if (!tp->check_cmd[0] && probs[tp->problem]->standard_checker[0]) {
          strcpy(tp->check_cmd, probs[tp->problem]->standard_checker);
          pathmake4(tp->check_cmd, global->ejudge_checkers_dir,
                    "/", tp->check_cmd, 0);
          tp->standard_checker_used = 1;
        }
        if (!tp->check_cmd[0] && atp && atp->check_cmd[0]) {
          sformat_message(tp->check_cmd, PATH_MAX, atp->check_cmd,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.check_cmd inherited from tester.%s ('%s')",
               i, sish, tp->check_cmd);        
        }
        if (!testers[i]->check_cmd[0]) {
          err("tester.%d.check_cmd must be set", i);
          return -1;
        }
        pathmake4(testers[i]->check_cmd, global->checker_dir, "/",
                  testers[i]->check_cmd, 0);
        if (!tp->start_cmd[0] && atp && atp->start_cmd[0]) {
          sformat_message(tp->start_cmd, PATH_MAX, atp->start_cmd,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.start_cmd inherited from tester.%s ('%s')",
               i, sish, tp->start_cmd);        
        }
        if (testers[i]->start_cmd[0]) {
          pathmake4(testers[i]->start_cmd, global->script_dir, "/",
                    testers[i]->start_cmd, 0);
        }
        if (!tp->prepare_cmd[0] && atp && atp->prepare_cmd[0]) {
          sformat_message(tp->prepare_cmd, PATH_MAX, atp->prepare_cmd,
                          global, probs[tp->problem], NULL,
                          tp, NULL, 0, 0, 0);
          info("tester.%d.prepare_cmd inherited from tester.%s ('%s')",
               i, sish, tp->prepare_cmd);        
        }
        if (tp->prepare_cmd[0]) {
          pathmake4(tp->prepare_cmd, global->script_dir, "/",
                    tp->prepare_cmd, 0);
        }
      }
    }
  }

  if (mode == PREPARE_SERVE) {
    /* check language/checker pairs */
    for (i = 1; i <= max_lang; i++) {
      if (!langs[i]) continue;
      for (j = 1; j <= max_prob; j++) {
        if (!probs[j]) continue;
        if (!find_tester(j, langs[i]->arch)) {
          err("no tester for pair: %d, %s", j, langs[i]->arch);
          return -1;
        }
      }
    }
  }

  return 0;
}

static int
collect_sections(int mode)
{
  struct generic_section_config *p;
  struct section_language_data  *l;
  struct section_problem_data   *q;
  struct section_tester_data    *t;
  int last_lang = 0, last_prob = 0, last_tester = 0;

  max_lang = max_prob = max_tester = 0;

  for (p = config; p; p = p->next) {
    if (!strcmp(p->name, "language") && mode != PREPARE_RUN) {
      l = (struct section_language_data*) p;
      if (!l->id) info("assigned language id = %d", (l->id = last_lang + 1));
      if (l->id <= 0 || l->id > MAX_LANGUAGE) {
        err("language id %d is out of range", l->id);
        return -1;
      }
      if (langs[l->id]) {
        err("duplicated language id %d", l->id);
        return -1;
      }
      langs[l->id] = l;
      if (l->id > max_lang) max_lang = l->id;
      last_lang = l->id;
      if (!l->compile_id) l->compile_id = l->id;
    } else if (!strcmp(p->name, "problem") && mode != PREPARE_COMPILE) {
      q = (struct section_problem_data*) p;
      if (q->abstract) {
        if (max_abstr_prob > MAX_PROBLEM) {
          err("too many abstract problems");
          return -1;
        }
        abstr_probs[max_abstr_prob++] = q;
      } else {
        if (!q->id) info("assigned problem id = %d", (q->id=last_prob + 1));
        if (q->id <= 0 || q->id > MAX_PROBLEM) {
          err("problem id %d is out of range", q->id);
          return -1;
        }
        if (probs[q->id]) {
          err("duplicated problem id %d", q->id);
          return -1;
        }
        probs[q->id] = q;
        if (q->id > max_prob) max_prob = q->id;
        last_prob = q->id;
        if (!q->tester_id) q->tester_id = q->id;
      }
    } else if (!strcmp(p->name, "tester") && mode != PREPARE_COMPILE) {
      t = (struct section_tester_data *) p;
      if (t->abstract) {
        if (max_abstr_tester > MAX_TESTER) {
          err("too many abstract tester");
          return -1;
        }
        abstr_testers[max_abstr_tester++] = t;
      } else {
        if (!t->id)
          info("assigned tester id = %d",(t->id = last_tester + 1));
        if (t->id <= 0 || t->id > MAX_TESTER) {
          err("tester id %d is out of range", t->id);
          return -1;
        }
        if (testers[t->id]) {
          err("duplicated tester id %d", t->id);
          return -1;
        }
        if (t->any) {
          int j;
          // default tester
          // its allowed to have only one for a given architecture
          for (j = 1; j <= max_tester; j++) {
            if (!testers[j] || j == t->id) continue;
            if (testers[j]->any == 1 && !strcmp(testers[j]->arch, t->arch))
              break;
          }
          if (j <= max_tester) {
            err("duplicated default tester for architecture '%s'", t->arch);
            return -1;
          }
        } else {
          if (!t->problem && !t->problem_name[0]) {
            err("no problem specified for tester %d", t->id);
            return -1;
          }
          if (t->problem && t->problem_name[0]) {
            err("only one of problem id and problem name must be specified");
            return -1;
          }
          if (t->problem && !probs[t->problem]) {
            err("no problem %d for tester %d", t->problem, t->id);
            return -1;
          }
          if (t->problem_name[0]) {
            int j;
            
            for (j = 1; j <= max_prob; j++) {
              if (probs[j] && !strcmp(probs[j]->short_name, t->problem_name))
                break;
            }
            if (j > max_prob) {
              err("no problem %s for tester %d", t->problem_name, t->id);
              return -1;
            }
            info("tester %d: problem '%s' has id %d",
                 t->id, t->problem_name, j);
            t->problem = j;
          }
        }
        testers[t->id] = t;
        if (t->id > max_tester) max_tester = t->id;
        last_tester = t->id;
      }
    }
  }
  return 0;
}

int
create_dirs(int mode)
{
  int i;

  if (mode == PREPARE_SERVE) {
    if (global->root_dir[0] && make_dir(global->root_dir, 0) < 0) return -1;
    if (make_dir(global->var_dir, 0) < 0) return -1;

    /* COMPILE writes its response here */
    if (make_dir(global->compile_dir, 0) < 0) return -1;
    if (make_all_dir(global->compile_queue_dir, 0777) < 0) return -1;
    if (make_dir(global->compile_src_dir, 0) < 0) return -1;
    if (make_dir(global->compile_out_dir, 0) < 0) return -1;
    if (make_all_dir(global->compile_status_dir, 0) < 0) return -1;
    if (make_dir(global->compile_report_dir, 0) < 0) return -1;

    /* RUN writes its response here */
    if (make_dir(global->run_dir, 0) < 0) return -1;
    if (make_all_dir(global->run_queue_dir, 0) < 0) return -1;
    if (make_dir(global->run_exe_dir, 0) < 0) return -1;
    if (make_dir(global->run_out_dir, 0) < 0) return -1;
    if (make_all_dir(global->run_status_dir, 0777) < 0) return -1;
    if (make_dir(global->run_report_dir, 0777) < 0) return -1;
    if (global->team_enable_rep_view) {
      if (make_dir(global->run_team_report_dir, 0777) < 0) return -1;
    }

    /* SERVE's status directory */
    if (make_all_dir(global->status_dir, 0) < 0) return -1;

    /* working directory (if somebody needs it) */
    if (make_dir(global->work_dir, 0) < 0) return -1;
    if (make_dir(global->print_work_dir, 0) < 0) return -1;
    if (make_dir(global->diff_work_dir, 0) < 0) return -1;

    /* SERVE's archive directories */
    if (make_dir(global->archive_dir, 0) < 0) return -1;
    if (make_dir(global->clar_archive_dir, 0) < 0) return -1;
    if (make_dir(global->run_archive_dir, 0) < 0) return -1;
    if (make_dir(global->report_archive_dir, 0) < 0) return -1;
    if (global->team_enable_rep_view) {
      if (make_dir(global->team_report_archive_dir, 0) < 0) return -1;
    }
    if (make_dir(global->team_extra_dir, 0) < 0) return -1;
  } else if (mode == PREPARE_COMPILE) {
    if (global->root_dir[0] && make_dir(global->root_dir, 0) < 0) return -1;
    if (make_dir(global->var_dir, 0) < 0) return -1;

    /* COMPILE reads its commands from here */
    if (make_dir(global->compile_dir, 0) < 0) return -1;
    if (make_all_dir(global->compile_queue_dir, 0777) < 0) return -1;
    if (make_dir(global->compile_src_dir, 0) < 0) return -1;

    /* working directory (if somebody needs it) */
    if (make_dir(global->work_dir, 0) < 0) return -1;
    if (make_dir(global->compile_work_dir, 0) < 0) return -1;
  } else if (mode == PREPARE_RUN) {
    if (global->root_dir[0] && make_dir(global->root_dir, 0) < 0) return -1;
    if (make_dir(global->var_dir, 0) < 0) return -1;

    /* RUN reads its commands from here */
    if (make_dir(global->run_dir, 0) < 0) return -1;
    if (make_all_dir(global->run_queue_dir, 0777) < 0) return -1;
    if (make_dir(global->run_exe_dir, 0) < 0) return -1;

    if (make_dir(global->work_dir, 0) < 0) return -1;
    if (make_dir(global->run_work_dir, 0) < 0) return -1;
    if (make_dir(global->run_check_dir, 0) < 0) return -1;
  }

  for (i = 1; i <= max_lang; i++) {
    if (!langs[i]) continue;
    if (mode == PREPARE_SERVE) {
      if (make_dir(langs[i]->compile_dir, 0) < 0) return -1;
      if (make_all_dir(langs[i]->compile_queue_dir, 0777) < 0) return -1;
      if (make_dir(langs[i]->compile_src_dir, 0) < 0) return -1;
      if (make_symlink(global->compile_out_dir, langs[i]->compile_out_dir) < 0)
        return -1;
    }
  }

  for (i = 1; i <= max_tester; i++) {
    if (!testers[i]) continue;
    if (mode == PREPARE_SERVE) {
      if (make_dir(testers[i]->run_dir, 0) < 0) return -1;
      if (make_all_dir(testers[i]->run_queue_dir, 0777) < 0) return -1;
      if (make_dir(testers[i]->run_exe_dir, 0) < 0) return -1;
      if (make_symlink(global->run_out_dir, testers[i]->run_out_dir) < 0)
        return -1;
    }
    if (mode == PREPARE_RUN) {
      if (testers[i]->any) continue;
      if (make_dir(testers[i]->check_dir, 0) < 0) return -1;
    }
  }

  write_log(0, LOG_INFO, "all directories created");
  return 0;
}

int
parse_version_string(int *pmajor, int *pminor, int *ppatch, int *pbuild)
{
  const unsigned char *p = compile_version;
  int n;

  if (sscanf(p, "%d.%dpre%d #%d%n", pmajor, pminor, ppatch, pbuild, &n) == 4
      && !p[n]) {
    *ppatch = -*ppatch;
  } else if (sscanf(p, "%d.%dpre%d%n", pmajor, pminor, ppatch, &n) == 3
             && !p[n]) {
    *ppatch = -*ppatch;
    *pbuild = 0;
  } else if (sscanf(p, "%d.%d.%d #%d%n", pmajor, pminor, ppatch, pbuild, &n)==4
             && !p[n]) {
  } else if (sscanf(p, "%d.%d.%d%n", pmajor, pminor, ppatch, &n) == 3
             && !p[n]) {
    *pbuild = 0;
  } else {
    err("cannot parse version string %s", compile_version);
    return -1;
  }
  if (*pmajor < 2 || *pmajor > 1000) return -1;
  if (*pminor < 0 || *pminor > 1000) return -1;
  return 0;
}

int
prepare(char const *config_file, int flags, int mode, char const *opts,
        int managed_flag)
{
  cfg_cond_var_t *cond_vars;
  int ncond_var;
  int major, minor, patch, build;

  if (parse_version_string(&major, &minor, &patch, &build) < 0) return -1;

  // initialize predefined variables
  ncond_var = 7;
  XALLOCAZ(cond_vars, ncond_var);
  cond_vars[0].name = "host";
  cond_vars[0].val.tag = PARSECFG_T_STRING;
  cond_vars[0].val.s.str = os_NodeName();
  cond_vars[1].name = "mode";
  cond_vars[1].val.tag = PARSECFG_T_LONG;
  cond_vars[1].val.l.val = mode;
  cond_vars[2].name = "major";
  cond_vars[2].val.tag = PARSECFG_T_LONG;
  cond_vars[2].val.l.val = major;
  cond_vars[3].name = "minor";
  cond_vars[3].val.tag = PARSECFG_T_LONG;
  cond_vars[3].val.l.val = minor;
  cond_vars[4].name = "patch";
  cond_vars[4].val.tag = PARSECFG_T_LONG;
  cond_vars[4].val.l.val = patch;
  cond_vars[5].name = "build";
  cond_vars[5].val.tag = PARSECFG_T_LONG;
  cond_vars[5].val.l.val = build;
  cond_vars[5].name = "managed";
  cond_vars[5].val.tag = PARSECFG_T_LONG;
  cond_vars[5].val.l.val = managed_flag;

  if ((flags & PREPARE_USE_CPP)) {
    FILE   *f = 0;
    path_t  cmd;
    /* invoke a preprocessor. */
    /* FIXME: check for preprocessor invokation variants? */
    /* FIXME: use task_New, etc...? */
    pathcpy(cmd, "cpp ");
    pathcat(cmd, opts);
    pathcat(cmd, " ");
    pathcat(cmd, config_file);
    if (!(f = popen(cmd, "r"))) {
      err("popen(\"%s\") failed: %s", cmd, os_ErrorMsg());
      return -1;
    }
    config = parse_param(NULL, f, params, 1, ncond_var, cond_vars);
    f = 0;
  } else {
    config = parse_param(config_file, 0, params, 1, ncond_var, cond_vars);
  }
  if (!config) return -1;
  write_log(0, LOG_INFO, "Configuration file parsed ok");
  if (collect_sections(mode) < 0) return -1;

  if (!max_lang && mode != PREPARE_RUN) {
    err("no languages specified");
    return -1;
  }
  if (!max_prob && mode != PREPARE_COMPILE) {
    err("no problems specified");
    return -1;
  }
  if (!max_tester && mode != PREPARE_COMPILE) {
    err("no testers specified");
    return -1;
  }
  if (set_defaults(mode) < 0) return -1;
  return 0;
}

int
prepare_tester_refinement(struct section_tester_data *out,
                          int def_tst_id, int prob_id)
{
  struct section_tester_data *tp, *atp = 0;
  struct section_problem_data *prb;
  int si, j;
  unsigned char *sish = 0;

  ASSERT(out);
  ASSERT(def_tst_id > 0 && def_tst_id <= max_tester);
  ASSERT(prob_id > 0 && prob_id <= max_prob);
  tp = testers[def_tst_id];
  prb = probs[prob_id];
  ASSERT(tp);
  ASSERT(tp->any);
  ASSERT(prb);

  // find abstract tester
  if (tp->super && tp->super[0]) {
    if (tp->super[1]) {
      err("concrete tester may inherit only one abstract tester");
      return -1;
    }

    for (si = 0; si < max_abstr_tester; si++) {
      atp = abstr_testers[si];
      if (!strcmp(atp->name, tp->super[0])) break;
    }
    if (si >= max_abstr_tester) {
      err("abstract tester '%s' not found", tp->super[0]);
      return -1;
    }
    sish = atp->name;
  }

  memset(out, 0, sizeof(*out));
  out->id = tp->id;
  out->problem = prob_id;

  /* copy architecture */
  strcpy(out->arch, tp->arch);
  if (!out->arch[0] && atp && atp->arch[0]) {
    strcpy(out->arch, atp->arch);
  }

  /* copy key */
  /* FIXME: key currently is not handled properly :-( */
  strcpy(out->key, tp->key);
  if (!out->key[0] && atp && atp->key[0]) {
    strcpy(out->key, atp->key);
  }

  /* generate tester name */
  /* FIXME: does the name matter? */
  /* FIXME: should we use the default tester's name? */
  if (out->arch[0]) {
    sprintf(out->name, "tst_dflt_%d_%d_%s", out->id, prob_id, out->arch);
  } else {
    sprintf(out->name, "tst_dflt_%d_%d", out->id, prob_id);
  }

  /* copy check_dir */
  strcpy(out->check_dir, tp->check_dir);
  if (!out->check_dir[0] && atp && atp->check_dir[0]) {
    sformat_message(out->check_dir, sizeof(out->check_dir),
                    atp->check_dir, global, prb, NULL, out, NULL, 0, 0, 0);
  }
  if (!out->check_dir[0]) {
    pathcpy(out->check_dir, global->run_check_dir);
  }

  /* copy no_core_dump */
  out->no_core_dump = tp->no_core_dump;
  if (out->no_core_dump == -1 && atp) {
    out->no_core_dump = atp->no_core_dump;
  }
  if (out->no_core_dump == -1) {
    out->no_core_dump = 0;
  }

  /* copy clear_env */
  out->clear_env = tp->clear_env;
  if (out->clear_env == -1 && atp) {
    out->clear_env = atp->clear_env;
  }
  if (out->clear_env == -1) {
    out->clear_env = 0;
  }

  /* copy time_limit_adjustment */
  out->time_limit_adjustment = tp->time_limit_adjustment;
  if (out->time_limit_adjustment == -1 && atp) {
    out->time_limit_adjustment = atp->time_limit_adjustment;
  }
  if (out->time_limit_adjustment == -1) {
    out->time_limit_adjustment = 0;
  }

  /* copy max_stack_size */
  out->max_stack_size = tp->max_stack_size;
  if (!out->max_stack_size && atp) {
    out->max_stack_size = atp->max_stack_size;
  }

  /* copy max_data_size */
  out->max_data_size = tp->max_data_size;
  if (!out->max_data_size && atp) {
    out->max_data_size = atp->max_data_size;
  }

  /* copy max_vm_size */
  out->max_vm_size = tp->max_vm_size;
  if (!out->max_vm_size && atp) {
    out->max_vm_size = atp->max_vm_size;
  }

  /* copy is_dos */
  out->is_dos = tp->is_dos;
  if (out->is_dos == -1 && atp) {
    out->is_dos = atp->is_dos;
  }
  if (out->is_dos == -1) {
    out->is_dos = 0;
  }

  /* copy priority_adjustment */
  out->priority_adjustment = tp->priority_adjustment;
  if (out->priority_adjustment == -1000 && atp) {
    out->priority_adjustment = atp->priority_adjustment;
  }
  if (out->priority_adjustment == -1000) {
    out->priority_adjustment = 0;
  }

  /* copy no_redirect */
  out->no_redirect = tp->no_redirect;
  if (out->no_redirect == -1 && atp) {
    out->no_redirect = atp->no_redirect;
  }
  if (out->no_redirect == -1) {
    out->no_redirect = 0;
  }

  /* copy kill_signal */
  strcpy(out->kill_signal, tp->kill_signal);
  if (!out->kill_signal[0] && atp) {
    strcpy(out->kill_signal, atp->kill_signal);
  }

  /* copy start_env */
  out->start_env = sarray_merge_pf(tp->start_env, out->start_env);
  if (atp && atp->start_env) {
    out->start_env = sarray_merge_pf(atp->start_env, out->start_env);
  }
  if (out->start_env) {
    for (j = 0; out->start_env[j]; j++) {
      out->start_env[j] = varsubst_heap(out->start_env[j], 1,
                                        section_global_params,
                                        section_problem_params,
                                        section_language_params,
                                        section_tester_params);
      if (!out->start_env[j]) return -1;
    }
  }

  /* copy checker_env */
  out->checker_env = sarray_merge_pf(tp->checker_env, out->checker_env);
  if (atp && atp->checker_env) {
    out->checker_env = sarray_merge_pf(atp->checker_env, out->checker_env);
  }
  if (out->checker_env) {
    for (j = 0; out->checker_env[j]; j++) {
      out->checker_env[j] = varsubst_heap(out->checker_env[j], 1,
                                          section_global_params,
                                          section_problem_params,
                                          section_language_params,
                                          section_tester_params);
      if (!out->checker_env[j]) return -1;
    }
  }

  /* copy errorcode_file */
  strcpy(out->errorcode_file, tp->errorcode_file);
  if (!out->errorcode_file[0] && atp && atp->errorcode_file[0]) {
    sformat_message(out->errorcode_file, sizeof(out->errorcode_file),
                    atp->errorcode_file, global, prb, NULL, out, NULL, 0, 0,0);
  }

  /* copy error_file */
  strcpy(out->error_file, tp->error_file);
  if (!out->error_file[0] && atp && atp->error_file[0]) {
    sformat_message(out->error_file, sizeof(out->error_file),
                    atp->error_file, global, prb, NULL, out, NULL, 0, 0, 0);
  }
  if (!out->error_file[0]) {
    snprintf(out->error_file, sizeof(out->error_file),
             "%s",  DFLT_T_ERROR_FILE);
  }

  /* copy check_cmd */
  if (prb->standard_checker[0]) {
    strcpy(out->check_cmd, prb->standard_checker);
    pathmake4(out->check_cmd,global->ejudge_checkers_dir,"/",out->check_cmd,0);
    out->standard_checker_used = 1;
  } else {
    strcpy(out->check_cmd, tp->check_cmd);
    if (!out->check_cmd[0] && atp && atp->check_cmd[0]) {
      sformat_message(out->check_cmd, sizeof(out->check_cmd),
                      atp->check_cmd, global, prb, NULL, out, NULL, 0, 0, 0);
    }
    if (!out->check_cmd[0]) {
      err("default tester for architecture '%s': check_cmd must be set",
          out->arch);
      return -1;
    }
    pathmake4(out->check_cmd, global->checker_dir, "/", out->check_cmd, 0);
  }

  /* copy start_cmd */
  strcpy(out->start_cmd, tp->start_cmd);
  if (!out->start_cmd[0] && atp && atp->start_cmd[0]) {
    sformat_message(out->start_cmd, sizeof(out->start_cmd),
                    atp->start_cmd, global, prb, NULL, out, NULL, 0, 0, 0);
  }
  if (out->start_cmd[0]) {
    pathmake4(out->start_cmd, global->script_dir, "/", out->start_cmd, 0);
  }

  /* copy prepare_cmd */
  strcpy(out->prepare_cmd, tp->prepare_cmd);
  if (!out->prepare_cmd[0] && atp && atp->prepare_cmd[0]) {
    sformat_message(out->prepare_cmd, sizeof(out->prepare_cmd),
                    atp->prepare_cmd, global, prb, NULL, out, NULL, 0, 0, 0);
  }
  if (out->prepare_cmd[0]) {
    pathmake4(out->prepare_cmd, global->script_dir, "/", out->prepare_cmd, 0);
  }

  // for debug
  //print_tester(stdout, out);

  return 0;
}

int
create_tester_dirs(struct section_tester_data *tst)
{
  ASSERT(tst);

  if (make_dir(tst->check_dir, 0) < 0) return -1;
  return 0;
}

void print_global(FILE *o)
{
  int i;
  struct config_parse_info *pp;

  for (i = 0; section_global_params[i].name; i++) {
    pp = &section_global_params[i];
    if (!strcmp(pp->type, "s")) {
      char *pc = XPDEREF(char,global, pp->offset);
      fprintf(o, "%s = \"%s\"\n", pp->name, pc);
    } else if (!strcmp(pp->type, "d")) {
      int *pi = XPDEREF(int,global, pp->offset);
      fprintf(o, "%s = %d\n", pp->name, *pi);
    }
  }
  fprintf(o, "\n");
}

void print_problem(FILE *o, struct section_problem_data *p)
{
  int i;
  struct config_parse_info *pp;

  fprintf(o, "[problem]\n");
  for (i = 0;; i++) {
    pp = &section_problem_params[i];
    if (!pp->name) break;
    if (!strcmp(pp->type, "s")) {
      char *pc = XPDEREF(char,p, pp->offset);
      if (!strcmp(pp->name, "super")) fprintf(o, "; ");
      fprintf(o, "%s = \"%s\"\n", pp->name, pc);
    } else if (!strcmp(pp->type, "d")) {
      int *pi = XPDEREF(int,p, pp->offset);
      fprintf(o, "%s = %d\n", pp->name, *pi);
    }
  }
  fprintf(o, "\n");
}
void print_all_problems(FILE *o)
{
  int i;

  for (i = 0; i < max_abstr_prob; i++)
    print_problem(o, abstr_probs[i]);
  for (i = 1; i <= max_prob; i++) {
    if (!probs[i]) continue;
    print_problem(o, probs[i]);
  }
}

void print_language(FILE *o, struct section_language_data *l)
{
  int i;
  struct config_parse_info *pp;

  fprintf(o, "[language]\n");
  for (i = 0;; i++) {
    pp = &section_language_params[i];
    if (!pp->name) break;
    if (!strcmp(pp->type, "s")) {
      char *pc = XPDEREF(char,l, pp->offset);
      fprintf(o, "%s = \"%s\"\n", pp->name, pc);
    } else if (!strcmp(pp->type, "d")) {
      int *pi = XPDEREF(int,l, pp->offset);
      fprintf(o, "%s = %d\n", pp->name, *pi);
    }
  }
  fprintf(o, "\n");
}
void print_all_languages(FILE *o)
{
  int i;

  for (i = 1; i <= max_lang; i++) {
    if (!langs[i]) continue;
    print_language(o, langs[i]);
  }
}

void print_tester(FILE *o, struct section_tester_data *t)
{
  int i;

  fprintf(o, "[tester]\n");
  if (t->abstract) fprintf(o, "abstract\n");
  fprintf(o, "name = \"%s\"\n", t->name);
  fprintf(o, "id = %d\n", t->id);
  if (t->super) {
    for (i = 0; t->super[i]; i++)
      fprintf(o, "; super = \"%s\"\n", t->super[i]);
    /*
    for (;t->super[i] != (char*) 1; i++)
      fprintf(o, "super = (null)\n");
    fprintf(o, "super = (1)\n");
    */
  }
  fprintf(o, "problem = %d\n", t->problem);
  fprintf(o, "problem_name = \"%s\"\n", t->problem_name);
  fprintf(o, "is_dos = %d\n", t->is_dos);
  fprintf(o, "no_redirect = %d\n", t->no_redirect);
  fprintf(o, "arch = \"%s\"\n", t->arch);
  fprintf(o, "key = \"%s\"\n", t->key);
  fprintf(o, "check_dir = \"%s\"\n", t->check_dir);
  fprintf(o, "no_core_dump = %d\n", t->no_core_dump);
  fprintf(o, "kill_signal = \"%s\"\n", t->kill_signal);
  fprintf(o, "max_stack_size = %d\n", t->max_stack_size);
  fprintf(o, "max_data_size = %d\n", t->max_data_size);
  fprintf(o, "max_vm_size = %d\n", t->max_vm_size);
  fprintf(o, "clear_env = %d\n", t->clear_env);
  fprintf(o, "errorcode_file = \"%s\"\n", t->errorcode_file);
  fprintf(o, "error_file = \"%s\"\n", t->error_file);
  fprintf(o, "prepare_cmd = \"%s\"\n", t->prepare_cmd);
  fprintf(o, "start_cmd = \"%s\"\n", t->start_cmd);
  fprintf(o, "check_cmd = \"%s\"\n", t->check_cmd);

  if (t->start_env) {
    for (i = 0; t->start_env[i]; i++)
      fprintf(o, "start_env = \"%s\"\n", t->start_env[i]);
    /*
    for (;t->start_env[i] != (char*) 1; i++)
      fprintf(o, "start_env = (null)\n");
    fprintf(o, "start_env = (1)\n");
    */
  }
  fprintf(o, "\n");
}
void print_all_testers(FILE *o)
{
  int i;

  /*
  fprintf(stderr, "====%d, %d, %u\n", max_abstr_tester, max_tester,
          sizeof(struct section_tester_data));
  */

  for (i = 0; i < max_abstr_tester; i++)
    print_tester(o, abstr_testers[i]);
  for (i = 1; i <= max_tester; i++) {
    if (!testers[i]) continue;
    print_tester(o, testers[i]);
  }

  fflush(o);
}

void print_configuration(FILE *o)
{
  print_global(o);
  print_all_problems(o);
  print_all_languages(o);
  print_all_testers(o);
}

/**
 * Local variables:
 *  compile-command: "make"
 *  c-font-lock-extra-types: ("\\sw+_t" "FILE")
 * End:
 */
