# the compiler: gcc for C program
CC = gcc

#compiler flags:
# -g adds debugging information to the executable file
# -Wall turns on most, but not all, compiler warnings
CFLAGS = -w -pg
DBGCFLAGS = -g -w -fopenmp
OPENMP = -fopenmp
# -lm links the math library
#LIBS = -lm -lpthread -lz
LIBS = -lm -pthread -lz
OPTIMIZATION = -march=native
#sources
SOURCES = ancestralclust.c options.c math.c opt.c
NEEDLEMANWUNSCH = needleman_wunsch.c alignment.c alignment_scoring.c
HASHMAP = hashmap.c
KALIGN = kalign/run_kalign.c kalign/tlmisc.c kalign/tldevel.c kalign/parameters.c kalign/rwalign.c kalign/alignment_parameters.c kalign/idata.c kalign/aln_task.c kalign/bisectingKmeans.c kalign/esl_stopwatch.c kalign/aln_run.c kalign/alphabet.c kalign/pick_anchor.c kalign/sequence_distance.c kalign/euclidean_dist.c kalign/aln_mem.c kalign/tlrng.c kalign/aln_setup.c kalign/aln_controller.c kalign/weave_alignment.c kalign/aln_seqseq.c kalign/aln_profileprofile.c kalign/aln_seqprofile.c
WFA = WFA/mm_allocator.c WFA/vector.c WFA/affine_wavefront.c WFA/affine_wavefront_penalties.c WFA/edit_cigar.c WFA/affine_wavefront_reduction.c WFA/affine_wavefront_align.c WFA/affine_wavefront_utils.c WFA/string_padded.c WFA/affine_wavefront_extend.c WFA/affine_wavefront_backtrace.c
OBJECTS = (SOURCES: .c = .o)
# the build target executable:
TARGET = ancestralclust

all: $(TARGET)
$(TARGET): $(TARGET).c
	$(CC) $(OPTIMIZATION) $(OPENMP) -o $(TARGET) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA) $(SOURCES) $(LIBS)
debug: $(TARGET).c
	$(CC) $(DBGCFLAGS) -o $(TARGET) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA) $(SOURCES) $(LIBS)

clean:
	$(RM) $(TARGET)

