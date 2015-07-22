#ifndef _PROFILER_H_
#define _PROFILER_H_

#ifndef PROF_ENABLE
    #define PROF_ENABLE 1
#endif

/*
 * When profiling, sometimes situation is possible, when profiled function calls recursively itself, in this situation profiling results may be distorted or look incorrect.
 * The following flag allows to track the level of recursion of profiled gaps.
 */
#define ENABLE_TRACING_RECURSION_DEPTH         1

/*
 *
 */
//#define TIME_WIDTH_IN_BITS                     32
#define TIME_WIDTH_IN_BITS                     64

/*
 * The following defines corresopnd to profiling gaps. For using profiler, you can rename them or add new. The only thing should be observed is GAP_CNT should be defined as
 * real amount of profiling gaps and gaps should be numbered sequentially.
 */
	#define GAP_0                          0
	#define GAP_1                          1
	#define GAP_2                          2
	#define GAP_3                          3
	#define GAP_4                          4
	#define GAP_5                          5
	#define GAP_6                          6
	#define GAP_7                          7
	#define GAP_8                          8
	#define GAP_9                          9
	#define GAP_CNT                       10

/*
 * Here is a type for printf-like function, it is necessary to allow to profiler to work in environments, where there is no printf and user provides printf-like function for
 * printing information.
 */
typedef int(*printf_p)(const char * format, ...);

/*
 * Here is a type for get_ticks like function. Obviously, profiler should be allowed to know current time.
 */
#if (TIME_WIDTH_IN_BITS == 32)
typedef unsigned int (*get_current_time_p)(void);
#else
typedef unsigned long long (*get_current_time_p)(void);
#endif
/*
 * This struct is element of array of profiling structures. Most of fields of this struct are related to particular gap only, but some fields are "global" i.e. they relate to
 * whole profiling, not to particuar record. They are duplicated in every record, but really used only in record #0.
 */
typedef struct
{
	unsigned long long     cummulative_clock;
	unsigned long long     start_clock;
	unsigned long long     gap_clock;
	unsigned long long     min_clock;
	unsigned long long     sup_min_clock;
	unsigned long long     sub_max_clock;
	unsigned long long     max_clock;
	unsigned long          counter;
	float                  percentage;
	unsigned               global_flag_outer_incorrect:1;  //we entered outer gap twice (possible due to strange recursion or stack corruption)
	signed                 global_stack_depth:9;           //amount of nested gaps plus 1
	unsigned               global_flag_stack_underrun:1;
	unsigned               local_flag_outer:1;             //this gap is outer (the only gap may be outer)
	unsigned               local_flag_stack_underrun:1;
	unsigned               local_flag_suspended:1;
	signed                 local_stack_depth:9;
	unsigned               local_max_recursion:8;
}profiler_entry_t;

/*
 * This is a declaration of main data structure, where all profiling data is stored. Surely, this array should be declared in one of .c -files.
 *
 */
extern volatile profiler_entry_t prof[GAP_CNT];

#if PROF_ENABLE
        /*
         *  This function should be called before all other methods of profiler.
         */
	static __inline__ void 
	init_prof(volatile profiler_entry_t *prof, unsigned count)
	{
		int i;
		for (i = 0; i < count; i++)
		{
			prof[i].start_clock = 0;
			prof[i].cummulative_clock = 0;
			prof[i].counter = 0;
			prof[i].global_flag_outer_incorrect = 0;
			prof[i].global_stack_depth = 0;
			prof[i].global_flag_stack_underrun = 0;
			prof[i].min_clock = (unsigned long long)(-1);
			prof[i].sup_min_clock = (unsigned long long)(-1);

			prof[i].max_clock = 0;
			prof[i].sub_max_clock = 0;

			prof[i].local_flag_outer = 0;
			prof[i].local_flag_suspended = 0;
			prof[i].local_flag_stack_underrun = 0;
			prof[i].local_stack_depth = 0;
			prof[i].local_max_recursion = 0;
		}
	}

/*
outer - gap, which is considered as 100% of time
*/
	static __inline__ void
	terminate_prof(volatile profiler_entry_t *prof, unsigned count, unsigned ref_gap)
	{
		unsigned i;
		long long total_ticks = 0;
		for (i = 0; i < count; i++)
		{
			//if (prof[i].local_flag_outer)
			if ((i == ref_gap) || (-1 == ref_gap))
			{
				total_ticks += prof[i].cummulative_clock;
			}
		}
		for (i = 0; i < count; i++)
		{
			if (!prof[0].global_flag_outer_incorrect)
			{
				prof[i].percentage = (100.0*(float)(prof[i].cummulative_clock)) / ((float)(total_ticks));
			}
			else
			{
				prof[i].percentage = -1.0;
			}
		}
	}

    
	static __inline__ void 
	start_prof(volatile profiler_entry_t *prof, unsigned gap_id, get_current_time_p get_ticks)
	{
		if (gap_id == -1) return;
		if (prof[gap_id].local_flag_outer)
		{
			if (prof[0].global_stack_depth)
			{
				dsp_printf("prof[%d].local_flag_outer = 1 when entering to gap_id=%d\n", gap_id, gap_id);
				prof[0].global_flag_outer_incorrect = 1;
			}
		}
		else
		{
			if (prof[0].global_stack_depth == 0)
			{
				prof[gap_id].local_flag_outer = 1;
			}
		}

		prof[0].global_stack_depth++;

		if (prof[gap_id].local_stack_depth == 0)
		{
			prof[gap_id].start_clock = get_ticks();
			prof[gap_id].counter++;
		}

		prof[gap_id].local_stack_depth++;

#if ENABLE_TRACING_RECURSION_DEPTH
		if (prof[gap_id].local_stack_depth > prof[gap_id].local_max_recursion)
		{
			prof[gap_id].local_max_recursion = prof[gap_id].local_stack_depth;
		}
#endif
	}

	static __inline__ void
	resume_prof(volatile profiler_entry_t *prof, unsigned gap_id, get_current_time_p get_ticks)
	{
		int ctr = prof[gap_id].counter;
		if (gap_id == -1) return;
		start_prof(prof, gap_id, get_ticks);
		prof[gap_id].counter = ctr;
		prof[gap_id].local_flag_suspended = 0;
	}

	static __inline__ void 
	stop_prof(volatile profiler_entry_t *prof, unsigned gap_id, get_current_time_p get_ticks)
	{
		if (gap_id == -1) return;
		prof[gap_id].local_stack_depth--;
		prof[0].global_stack_depth--;
		if (prof[gap_id].local_stack_depth == 0)
		{
			prof[gap_id].gap_clock += (get_ticks() - prof[gap_id].start_clock);
			prof[gap_id].cummulative_clock += prof[gap_id].gap_clock;
			if (!prof[gap_id].local_flag_suspended)
			{
				if (prof[gap_id].gap_clock > prof[gap_id].max_clock)
				{
					prof[gap_id].sub_max_clock = prof[gap_id].max_clock;
					prof[gap_id].max_clock = prof[gap_id].gap_clock;
				}
				if (prof[gap_id].gap_clock < prof[gap_id].min_clock)
				{
					prof[gap_id].sup_min_clock = prof[gap_id].min_clock;
					prof[gap_id].min_clock = prof[gap_id].gap_clock;
				}
				prof[gap_id].gap_clock = 0;
			}

			if (prof[gap_id].local_flag_outer)
			{
 				if (prof[0].global_stack_depth != 0)
				{
					prof[0].global_flag_outer_incorrect = 1;
				}
				else
				{
					prof[gap_id].local_flag_outer = 0;
				}
			}
			else
			{
				if (prof[0].global_stack_depth == 0)
				{
					prof[0].global_flag_outer_incorrect = 1;
				}
			}
		}
		if (prof[0].global_stack_depth < 0)
		{
			prof[0].global_flag_stack_underrun = 1;
		}
		if (prof[gap_id].local_stack_depth < 0)
		{
			prof[gap_id].local_flag_stack_underrun = 1;
		}
	}


	static __inline__ void
	suspend_prof(volatile profiler_entry_t *prof, unsigned gap_id, get_current_time_p get_ticks)
	{
		if (gap_id == -1) return;
		prof[gap_id].local_flag_suspended = 1;
		stop_prof(prof, gap_id, get_ticks);
	}

	static __inline__ void
	stat_header(volatile profiler_entry_t *prof, printf_p pf)
	{
		pf("\n\ntiny-prof statistics:\n====================\n");
 		if (prof[0].global_flag_outer_incorrect)
		{
			pf("outer is incorrect, i.e. intervals may be partially interleave\n");
			pf("(cannot calculate percentage due to incorrect nesting, -1.0 will be printed)\n\n");
		}
		if (prof[0].global_flag_stack_underrun)
		{
			pf("Profiler's stack underrun! Probably some intervals aren't balanced properly.\n\n");
		}
		pf(" id |                           gap |       ticks |    count | min.ticks | ave.ticks |  max.ticks | percentage | recur |notes\n");
		pf("----+-------------------------------+-------------+----------+-----------+-----------+------------+------------+-------+-----\n");
	}

	#define CORRECT_GAP(gap_id)  (gap_id == -1? 0 : gap_id)
	#define STAT_PRINT(prof, gap_id, pf, promt) \
	do \
	{  \
		long long aver = (prof[CORRECT_GAP(gap_id)].counter != 0 ? \
		                  prof[CORRECT_GAP(gap_id)].cummulative_clock/(long long)prof[CORRECT_GAP(gap_id)].counter : \
		                  0 ); \
		unsigned long long     cummulative_clock = prof[CORRECT_GAP(gap_id)].cummulative_clock; \
		unsigned long long     min_clock = prof[CORRECT_GAP(gap_id)].min_clock; \
		unsigned long long     max_clock = prof[CORRECT_GAP(gap_id)].max_clock; \
		unsigned long          counter = prof[CORRECT_GAP(gap_id)].counter; \
		float                  percentage = prof[CORRECT_GAP(gap_id)].percentage; \
		unsigned               local_max_recursion = prof[CORRECT_GAP(gap_id)].local_max_recursion; \
		unsigned               local_flag_stack_underrun = prof[CORRECT_GAP(gap_id)].local_flag_stack_underrun; \
		if (gap_id == -1) break; \
		pf("%3s |%30s |%12lld |%9d |%10lld |%10lld |%11lld |     % 6.1f | % 5d | %s\n", \
		    promt,\
		    #gap_id, \
		    cummulative_clock, \
		    counter, \
		    min_clock, \
		    aver, \
		    max_clock, \
		    percentage, \
		    local_max_recursion,\
		    (local_flag_stack_underrun ? "starts and ends arn't balanced" : "")); \
	}while(0)

	#define STAT_PRINT_NO_EXTREMALS(prof, gap_id, pf, promt) \
	do \
	{  \
		long long aver = (prof[gap_id].counter > 4 ? \
		                 (prof[gap_id].cummulative_clock - \
		                  prof[gap_id].max_clock - \
		                  prof[gap_id].sub_max_clock - \
		                  prof[gap_id].min_clock - \
		                  prof[gap_id].sup_min_clock) \
		                  /(long long)(prof[gap_id].counter-4) : \
		                  0 ); \
		pf("%3s |%30s |%12lld |%9d |%10lld |%10lld |%11lld |     % 6.1f | % 5d | %s\n", \
		    promt,\
		#gap_id, \
		prof[gap_id].cummulative_clock, \
		prof[gap_id].counter, \
		prof[gap_id].min_clock, \
		aver, \
		prof[gap_id].max_clock, \
		prof[gap_id].percentage, \
		prof[gap_id].local_max_recursion,\
		(prof[gap_id].local_flag_stack_underrun ? "starts and ends arn't balanced" : "")); \
	}while(0)

#else
	#define init_prof(A, B)
	#define terminate_prof(A, B)
	#define start_prof(A, B)
	#define stop_prof(A, B)
	#define stat_header(A, B)
	#define STAT_PRINT(A, B, C, D)
#endif

#endif





