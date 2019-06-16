
/* -*- mode: C; indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4 -*- */

#include "execute.h"
#include "config.h"

#define EXENAME "program"

MYSQL_RES*    MySQL_res;
MYSQL_ROW     MySQL_row;
MYSQL*        hMySQL;
char          query_str[100000];
double        spend_time;
int           spend_memory;
char          is_multiple_cases;
char          WORKDIR[100];
char          EXEPROG[100];
unsigned long status_id, problem_id, cproblem_id, user_id, cid, qid, current_case;
char*         language, *compiler, *ext, *ccmd;
time_t        contest_start_time, contest_end_time, submit_time;
unsigned long time_limit, memory_limit;
int           special_judge, has_framework;
char          exeprog[128];
char          srccode[100000];
char          compile_log[100000] = "";
char          sqllog[200001];

#define MAX(a, b)       ((a > b) ? (a) : (b))

#define set_source_path(path)   \
sprintf(path, "%s/%03lu/%06lu.%s", \
SRC_PREFIX, status_id / 1000, status_id, ext)

#define set_data_path(path, filename)   \
sprintf(path, "%s/%lu/%s",      \
DATA_PREFIX, problem_id, filename)

#define file_size(x)    (f_stat(x)->st_size)
#define file_exist(x)   (f_stat(x)->st_mtime != 0)

struct stat *f_stat(char *file)
{
    static struct stat buf;

    memset(&buf, 0, sizeof(buf));

    if (stat(file, &buf) < 0)
    {
        memset(&buf, 0, sizeof(buf));
    }

    return &buf;
}

void p_error(MYSQL *mysql)
{
    fprintf(stderr, "%s", mysql_error(mysql));
}

void halt(char *msg)
{
    fprintf(stderr, "%s\n", msg);

    if (hMySQL)
    {
        mysql_close(hMySQL);
    }

    exit(255);
}

void init()
{
    if ((hMySQL = mysql_init(NULL)) == NULL)
    {
        halt("mysql_init error");
    }

    if (mysql_real_connect(hMySQL, HOST, USER, PASSWD, DATABASE, 0, NULL, 0) == NULL)
    {
        p_error(hMySQL);
        halt("mysql_connect error");
    }

    int i;
    for (i = 0; ; ++i) {
        snprintf(WORKDIR, sizeof(WORKDIR), "/tmp/judger_%d", i);
        struct stat st;
        if(stat(WORKDIR,&st)) {
            // workdir doestn't exists.
            break;
        }
    }
    
    mkdir(WORKDIR, 0777);
    chdir(WORKDIR);
    snprintf(EXEPROG, sizeof(EXEPROG), "%s/%s", WORKDIR, EXENAME);

    if (!mysql_set_character_set(hMySQL, "utf8"))
    {
        printf("Charset is set to UTF8\n");
    }
    
    printf("Working folder: %s\n", WORKDIR);
}

void clear()
{
    if (MySQL_res)
    {
        mysql_free_result(MySQL_res);
    }

    MySQL_res = NULL;

    if (hMySQL)
    {
        mysql_close(hMySQL);
    }

    hMySQL = NULL;
}

void query(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf(query_str, fmt, ap);
    va_end(ap);

    query_str[99999] = 0;

    if (mysql_query(hMySQL, query_str) != 0)
    {
        p_error(hMySQL);
    }
}

int fetch_row()
{
    MySQL_res = mysql_store_result(hMySQL);

    if (MySQL_res == NULL)
    {
        p_error(hMySQL);
    }

    if (mysql_num_rows(MySQL_res) == 0)
    {
        mysql_free_result(MySQL_res);

        return 0;
    }

    MySQL_row = mysql_fetch_row(MySQL_res);

    return 1;
}

int fetch_queue()
{
    int i;

    query("SELECT qid,queue.sid,uid,pid,language,time,cid,cpid,sourcecode FROM queue,status WHERE queue.sid=status.sid AND hold=0 AND server_id=%d ORDER BY qid LIMIT 1",
          SERVER_ID);

    if (fetch_row() == 0)
    {
        return 0;
    }

    qid        = atoi(MySQL_row[0]);
    status_id  = atoi(MySQL_row[1]);
    user_id    = atoi(MySQL_row[2]);
    problem_id = atoi(MySQL_row[3]);
    language   = MySQL_row[4];

    if (MySQL_row[8] != NULL)
    {
        strncpy(srccode, MySQL_row[8], sizeof(srccode));
    }
    else
    {
        strcpy(srccode, "Fail to fetch code");
    }

    if (MySQL_row[6] != NULL)
    {
        cid         = atoi(MySQL_row[6]);
        cproblem_id = atoi(MySQL_row[7]);
    }
    else
    {
        cid = 0;
    }

    for (i = 0; lang[i].language; i++)
    {
        if (!strcmp(lang[i].language, language))
        {
            compiler = lang[i].compiler;
            ext      = lang[i].ext;
            ccmd     = lang[i].ccmd;

            break;
        }
    }

    struct tm mytm;

    if (MySQL_row[5] != NULL)
    {
        strptime(MySQL_row[5], "%Y-%m-%d %H:%M:%S", &mytm);

        submit_time = mktime(&mytm);
    }

    mysql_free_result(MySQL_res);

    return 1;
}

void update_user(int solved)
{
    char list[1051];
    int  i;

    query("SELECT sid FROM status WHERE pid='%lu' AND uid='%lu' AND status.status = 'Accepted' and sid != '%lu'",
          problem_id, user_id, status_id);
    printf("Update status pid=%lu uid=%lu sid=%lu solved=%d\n", problem_id, user_id, status_id, solved);

    if (!fetch_row() && solved)
    {
	query("SELECT * FROM stock_problems WHERE pid='%lu'", problem_id);
	if (fetch_row()) {
		query("UPDATE user SET stock_solved=stock_solved+1 WHERE uid='%lu'", user_id);
	}
    }

    if (solved)
    {
        query("UPDATE problems SET accepted = accepted + 1 WHERE pid='%lu'", problem_id);
    }

    if (cid)
    {
        printf("Contest %lu Submission\n", cid);
        query("SELECT accepted, ac_time, submissions FROM ranklist WHERE uid='%lu' AND cid='%lu' AND pid='%lu'",
              user_id, cid, cproblem_id);

        if (fetch_row() == 0)
        {
            puts("No Submissions");
            query("INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES ('%lu', '%lu', '%lu', '%d', 1, '%d')",
                  user_id, cid, cproblem_id, solved, (submit_time - contest_start_time) / 60 + 1);
        }
        else if (atoi(MySQL_row[0]) == 0)
        {
            puts("Update Non-ac Submissions");

            int submissions = atoi(MySQL_row[2]);

            query("UPDATE ranklist SET accepted='%d',ac_time='%d',submissions='%d' WHERE uid='%lu' AND cid='%lu' AND pid='%lu'",
                  solved, (submit_time - contest_start_time) / 60 + 1, submissions + 1, user_id, cid, cproblem_id);
        }
        else if ((atoi(MySQL_row[0]) == 1) && solved
                 && (atoi(MySQL_row[1]) > (submit_time - contest_start_time) / 60 + 1))
        {    // rejudge
            puts("Rejudge");
            query("DELETE FROM ranklist WHERE uid='%lu' AND cid='%lu' AND pid='%lu' AND ac_time>'%lu'", user_id, cid,
                  cproblem_id, (submit_time - contest_start_time) / 60 + 1);
            query("INSERT INTO ranklist (uid, cid, pid, accepted, submissions, ac_time) VALUES ('%lu', '%lu', '%lu', '%d', 1, '%d')",
                  user_id, cid, cproblem_id, solved, (submit_time - contest_start_time) / 60 + 1);
            query("UPDATE problems SET accepted=accepted+1 WHERE cid='%lu' AND pid='%lu'", cid, cproblem_id);
        }
    }
}

void set_status(const char *msg)
{
    static char runtime[32],
                memory[32];
    static int  final_status[9] =
    {
        14, 2, 9, 4, 6, 7, 5, 15, 16
    };
    int         i;

    memset(runtime, 0, sizeof(runtime));
    memset(memory, 0, sizeof(memory));

    for (i = 0; i < 7; ++i)
    {
        if (strcmp(msg, status[final_status[i]]) == 0)
        {
            break;
        }
    }

    if (i < 7)
    {
        snprintf(runtime, sizeof(runtime), ",run_time='%.2f'", spend_time);
        snprintf(memory, sizeof(memory), ",run_memory='%u'", spend_memory);
    }

    query("UPDATE status SET status='%s' ,failcase='%ld' %s %s WHERE sid='%lu'", msg,
          is_multiple_cases ? current_case : -1, (runtime[0]) ? runtime : "", (memory[0]) ? memory : "", status_id);
    printf("JUDGE: %s %d\n", msg, i);
}

int compile_framework(char *srcpath)
{
    char buf[512];

    strncpy(exeprog, EXEPROG, sizeof(exeprog));
    printf("exeprog: %s\n", exeprog);

    char fw_path[512];

    set_data_path(fw_path, "framework.cpp");

    if (!file_exist(fw_path))
    {
        printf("No framework source file provided.\n");

        return -1;
    }

    char fw_dst[256];
    char src_dst[256];

    snprintf(fw_dst, sizeof(fw_dst), "%s/framework.cpp", WORKDIR);
    snprintf(src_dst, sizeof(src_dst), "%s/source.cpp", WORKDIR);

    if (file_exist(fw_dst))
    {
        unlink(fw_dst);
    }

    if (file_exist(src_dst))
    {
        unlink(src_dst);
    }

    snprintf(buf, sizeof(buf), "cp %s %s", fw_path, fw_dst);
    system(buf);
    snprintf(buf, sizeof(buf), "cp %s %s", srcpath, src_dst);
    system(buf);
    snprintf(buf, sizeof(buf), ccmd, exeprog, fw_dst, srcpath);

    if (file_exist(exeprog))
    {
        unlink(exeprog);
    }

    printf("Compiling: %s\n", buf);
    system(buf);
    system("echo $LANG");

    if (file_exist(exeprog))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int compile_normal(char *srcpath)
{
    char buf[512];

    strncpy(exeprog, EXEPROG, sizeof(exeprog));
    printf("exeprog: %s\n", exeprog);
    snprintf(buf, sizeof(buf), ccmd, exeprog, srcpath, srcpath);

    if (file_exist(exeprog))
    {
        unlink(exeprog);
    }

    printf("Compiling: %s\n", buf);
    system(buf);

    if (file_exist(exeprog))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int compile_java(char *srcpath)
{
    char buf[512];
    char classprog[512];

    snprintf(classprog, sizeof(classprog), "%s/Main.class", WORKDIR);
    snprintf(exeprog, sizeof(exeprog), "/usr/bin/run_java");

    // snprintf(exeprog, sizeof(exeprog), "java -cp "WORKDIR" Main");
    snprintf(buf, sizeof(buf), ccmd, WORKDIR, srcpath, srcpath);

    if (file_exist(classprog))
    {
        unlink(classprog);
    }

    printf("Compiling: %s\n", buf);
    system(buf);

    if (file_exist(classprog))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int compile(char *srcpath)
{
	if (has_framework)
	{
		return compile_framework(srcpath);
	}
	else
	{
		return compile_normal(srcpath);
	}
}

void set_compilelog(char *srcpath)
{
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s.log", srcpath);

    if (!file_exist(buf))
    {
        printf("Error: compile log files not found.(%s)\n", buf);
    }

    FILE* logfile = fopen(buf, "r");

    compile_log[0] = 0;

    while (fgets(buf, sizeof(buf), logfile))
    {
        strncat(compile_log, buf, sizeof(compile_log));
    }

    mysql_real_escape_string(hMySQL, sqllog, compile_log, strlen(compile_log));
    query("UPDATE status set compilelog = '%s' where sid = '%lu'", sqllog, status_id);;
}

int diff(char *file1, char *file2, int exact_match)
{
    char cmd[256];

    if (!file_exist(file1))
    {
        return 1;
    }

    if (!file_exist(file2))
    {
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "diff %s %s %s > %s/diff.result", exact_match ? "--strip-trailing-cr" : "--ignore-trailing-space --strip-trailing-cr", file1, file2, WORKDIR);
    system(cmd);

    char buf[256];

    snprintf(buf, sizeof(buf), "%s/diff.result", WORKDIR);

    if (file_size(buf) == 0)
    {
        return 0;
    }

    return 1;
}

FILE* case_index;

void check_multiple_case(FILE *inf)
{
    int  num = 0;
    char inpath[1000], stdpath[1000];

    is_multiple_cases = 0;

    if (feof(inf))
    {
        return;
    }

    while (fscanf(inf, "%s %s", inpath, stdpath) != EOF)
    {
        ++num;
    }

    if (num > 1)
    {
        is_multiple_cases = 1;
    }
}

void init_testcase()
{
    char path[80];

    // if (cid)
    // snprintf(path, sizeof(path), "%s/c%02lu/%lu/.DIR", DATA_PREFIX,
    // cid, problem_id);
    // else
    snprintf(path, sizeof(path), "%s/%lu/.DIR", DATA_PREFIX, problem_id);

    case_index = fopen(path, "r");

    if (case_index == NULL)
    {
        fprintf(stderr, "Error in openning problem %lu index file", problem_id);

        return;
    }

    check_multiple_case(case_index);
    fclose(case_index);

    case_index = fopen(path, "r");

    if (cid)
    {
        query("SELECT starttime, during FROM contests WHERE cid='%lu'", cid);

        if (fetch_row() > 0)
        {
            struct tm mytm;

            strptime(MySQL_row[0], "%Y-%m-%d %T", &mytm);

            contest_start_time = mktime(&mytm);
            contest_end_time   = atol(MySQL_row[1]) * 3600 + contest_start_time;
        }
    }

    query("SELECT time_limit, memory_limit, special_judge, has_framework FROM problems WHERE pid='%lu'", problem_id);

    if (fetch_row() > 0)
    {
        time_limit   = atol(MySQL_row[0]);
        memory_limit = atol(MySQL_row[1]);

        if (!strcasecmp(MySQL_row[2], "1"))
        {
            special_judge = 1;
        }
        else
        {
            special_judge = 0;
        }

        if (!strcasecmp(MySQL_row[3], "1"))
        {
            has_framework = 1;
        }
        else
        {
            has_framework = 0;
        }
    }

    current_case = 0;
}

int select_case(char *infile, char *outfile, char *stdfile)
{
    char in[80], std[80];

    if (case_index == NULL)
    {
        return 0;
    }

    if (feof(case_index))
    {
        return 0;
    }

    if (fscanf(case_index, "%s %s", in, std) != EOF)
    {
        set_data_path(infile, in);
        set_data_path(stdfile, std);
        sprintf(outfile, "%s/%lu.out", WORKDIR, problem_id);

        if (file_exist(outfile))
        {
            unlink(outfile);
        }

        return 1;
    }

    return 0;
}

void cleanup()
{
    // delete file created in judging
    char buf[256];

    snprintf(buf, sizeof(buf), "%s/%lu.out", WORKDIR, problem_id);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/program", WORKDIR);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/diff.result", WORKDIR);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/source.cpp", WORKDIR);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/result", WORKDIR);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/framework.cpp", WORKDIR);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/%lu.%s", WORKDIR, status_id, ext);

    if (file_exist(buf))
    {
        unlink(buf);
    }

    snprintf(buf, sizeof(buf), "%s/%lu.%s.log", WORKDIR, status_id, ext);

    if (file_exist(buf))
    {
        unlink(buf);
    }
}

void end_testcase()
{
    fclose(case_index);

    case_index = NULL;
}

void clear_runstat()
{
    spend_memory = 0;
    spend_time   = 0;
}

void get_source(char *path)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%s/%lu.%s", WORKDIR, status_id, ext);
    printf("%s\n", buf);

    FILE* srcfile = fopen(buf, "w");

    fprintf(srcfile, "%s\n", srccode);
    fclose(srcfile);
    strncpy(path, buf, sizeof(buf));
}

void do_judge()
{
    int    runflag,
           solved = 0;
    double case_time;
    int    case_memory;
    char   src[128], in[128], out[128],
           std[128];
    int    pe = 0;

    init_testcase();

    // set_status(STAT_COMPILING); //compiling
    printf("Start to fetch source code from database\n");
    get_source(src);
    printf("Saved source code to %s\n", src);

    // set_source_path(src);
    if (compile(src) < 0)
    {
        set_status("Compile Error");
        set_compilelog(src);
        update_user(0);
        cleanup();

        return;
    }

    set_compilelog(src);
    clear_runstat();

    while (select_case(in, out, std))
    {
        set_status(STAT_JUDGING);

        runflag    = execute(in, out, exeprog, time_limit, memory_limit, 32000000, &case_memory, &case_time);
        spend_time += case_time;

        // FIXME: use spend_time here
        if (case_time > time_limit)
        {
            spend_memory = MAX(spend_memory, case_memory);

            set_status(STAT_TLE);
            goto judge_end;
        }

        if (runflag < 0)
        {
            switch (runflag)
            {
                case EXEC_RE:
                    set_status(STAT_RE);
                    goto judge_end;
                case EXEC_TLE:
                    spend_memory = MAX(spend_memory, case_memory);

                    set_status(STAT_TLE);
                    goto judge_end;
                case EXEC_MLE:
                    set_status(STAT_MLE);
                    goto judge_end;
                case EXEC_OLE:
                    set_status(STAT_OLE);
                    goto judge_end;
                case EXEC_RF:
                    set_status(STAT_RF);
                    goto judge_end;
				case EXEC_SF:
					set_status(STAT_RE);
					goto judge_end;
				case EXEC_FPE:
					set_status(STAT_RE);
					goto judge_end;
                default:
                    set_status(STAT_OTHER);
                    goto judge_end;
            }
        }

        if (special_judge)
        {
            // pid_t pid = fork();
            // if (pid == 0) {
            fprintf(stderr, "execute %s/%lu/spjudge %s %s\n", DATA_PREFIX, problem_id, in, out);

            char buf[256];

            unlink("/tmp/result");
            sprintf(buf, "%s/%lu/spjudge %s %s %s > /tmp/result", DATA_PREFIX, problem_id, in, out, std);
            system(buf);

            // execlp("/usr/local/sicily/testdata/c02/1009/spjudge", in, out, NULL);
            // } else {
            // int state;
            // waitpid(pid, &state, 0);
            // fprintf(stderr, "return state=%d\n", WEXITSTATUS(state));
            FILE* fpp = fopen("/tmp/result", "r");

            if (fpp == NULL)
            {
                set_status(STAT_WA);
                fclose(fpp);

                goto judge_end;
            }

            char result[10];

            fgets(result, sizeof(result), fpp);

            if ((result[0] == 'y') || (result[0] == 'Y'))
            {
                puts("Yes");
                fclose(fpp);

                // solved = 1;
                // set_status(STAT_AC);
                // goto judge_end;
            }
            else
            {
                set_status(STAT_WA);
                fclose(fpp);

                goto judge_end;
            }

            // if (WIFEXITED(state)){

            /*
             * if (WEXITSTATUS(state) == 0){
             *       solved = 1;
             *       set_status(STAT_AC);
             *       goto judge_end;
             * } else {
             *       set_status(STAT_WA);
             * }
             */

            // goto judge_end;
            // }
        }
        else
        {
            if (diff(out, std, 0) == 1)
            {
                set_status(STAT_WA);
                goto judge_end;
            }

            if (diff(out, std, 1) == 1)
            {
                pe = 1;
                set_status(STAT_PE);

                goto judge_end;
            }
        }

        // spend_time = MAX(spend_time, case_time);
        spend_memory = MAX(spend_memory, case_memory);

        ++current_case;
    }

    // if (pe == 1)
    // {
    // set_status(STAT_PE);
    // goto judge_end;
    // }
    set_status(STAT_AC);
    solved = 1;
judge_end:
    update_user(solved);
    end_testcase();
    cleanup();
}

void popup_queue()
{
    query("DELETE FROM queue WHERE qid='%lu'", qid);
}

int main(int argc, char **argv)
{
    parse_config(argc, argv);
    init();

    while (1)
    {
        if (fetch_queue() == 0)
        {
            // sleep(60);
            sleep(1);
        }
        else
        {
            popup_queue();
            printf("Start new judge\n");
            do_judge();
        }
    }

    /* won't reach here */
    clear();

    return 0;
}
