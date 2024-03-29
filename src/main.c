#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#define CODE_BUFFER_SIZE 400000
#define TAPE_SIZE 30000
#define LOOPSTACK_SIZE 256
#define IO_SIZE 1000000

// set to 1 to get a ~40% speed boost at the cost of some reliability
// segfaults 50% of the time.
#define FAST 1
// the number of bytes that need to be compiled before the compiler can start running
// the higher the safer (and slower)
// some bytes might allways stay zero and will cause an infinite loop. 
#define NUMBYTES 15000

#define PLUS '+'
#define MINUS '-'
#define RIGHT '>'
#define LEFT '<'
#define DISPLAY '.'
#define INPUT ','
#define LOOP_START '['
#define LOOP_END ']'

#define add_u8_to_pointer(pointer, u8) \
	*(pointer++) = (uint8_t)u8

#define add_u32_to_pointer(pointer, u32) \
	*(pointer++) = (uint8_t)(u32 & 0x000000ff);\
	*(pointer++) = (uint8_t)((u32 & 0x0000ff00) >> 8 );\
	*(pointer++) = (uint8_t)((u32 & 0x00ff0000) >> 16);\
	*(pointer++) = (uint8_t)((u32 & 0xff000000) >> 24)

#define add_u16_to_pointer(pointer, u16) \
	*(pointer++) = (uint8_t)(u32 & 0x00ff);\
	*(pointer++) = (uint8_t)((u32 & 0xff00) >> 8 );\

uint8_t * executable_buffer(size_t size){
	uint8_t *memory = mmap (NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED){
		return NULL;
	}
	return memory;
}

void compile(uint8_t * program, char * filename){
	FILE * file = fopen(filename,"r");
	if(file == NULL){
		printf("brainfuck file not found\n");
		exit(-1);
	}

	// find the filesize
	fseek(file, 0L, SEEK_END);
	size_t filesize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	// all the brainfuck characters
	char * brainfuck_code = malloc(filesize + 1);
	fread(brainfuck_code, 1, filesize, file);

	fclose(file);

	register char * result_iter = brainfuck_code;
	for(register char * iter = brainfuck_code; *iter != '\0'; iter++){
		switch(*iter){
			case RIGHT:
			case LEFT:
			case PLUS:
			case MINUS:
			case LOOP_END:
			case LOOP_START:
			case DISPLAY:
			case INPUT:
			case '\0':
				*(result_iter++) = *iter;
			default:;
		}
	}
	*result_iter = '\0';

	// points to the current position in the program that's
	// written to.
	#if FAST
	register uint8_t * programpointer = program;
	#else
	// Add one so the first byte stays a nop (in safe mode)
	register uint8_t * programpointer = program + 1;
	#endif

	// stack for keeping track of loop jump locations
	uint8_t ** loopstack = malloc(LOOPSTACK_SIZE * sizeof(uint8_t *));
	register uint16_t loopstackpointer = 0;

	for(register char * iter = brainfuck_code; *iter != '\0';){
		register char * count = iter;

		switch(*iter){
			case RIGHT:;
				while(*iter == RIGHT){
					iter++;
				}
				
				// add instruction prefix
				add_u8_to_pointer(programpointer, 0x48);
				add_u8_to_pointer(programpointer, 0x81);
				add_u8_to_pointer(programpointer, 0xc3);
				// number to add (4 bytes)
				add_u32_to_pointer(programpointer, (uint32_t)(iter-count));

				break;
			case LEFT:;
				int leftcounter = 0;
				while(*iter == LEFT){
					iter++;
					leftcounter++;
				}
				if(*iter == PLUS){
					char * tmpiter = iter+1;
					int rightcounter = 0;

					// char * string = malloc(45);
					// memcpy(string, tmpiter-20, 40);
					// string[40] = 0;
					// printf("%s\n", string);

					while(*tmpiter == RIGHT && rightcounter != leftcounter){
						tmpiter++;
						rightcounter++;
					}
					
					if(rightcounter == leftcounter){
						add_u8_to_pointer(programpointer, 0xfe);
						add_u8_to_pointer(programpointer, 0x83);
						add_u32_to_pointer(programpointer, -rightcounter);
						iter = tmpiter;
						break;
					}
				}

				// add instruction prefix
				add_u8_to_pointer(programpointer, 0x48);
				add_u8_to_pointer(programpointer, 0x81);
				add_u8_to_pointer(programpointer, 0xeb);
				// number to subtract (4 bytes)
				add_u32_to_pointer(programpointer, (uint32_t)(iter-count));

				break;
			case PLUS:;
				while(*iter == PLUS){
					iter++;
				}
				// add instruction prefix
				add_u8_to_pointer(programpointer, 0x80);
				add_u8_to_pointer(programpointer, 0x03);
				// number to add. (must be byte as cell size is 1 byte)
				add_u8_to_pointer(programpointer, (uint8_t)((iter-count) & 0xff));

				break;
			case MINUS:;
				while(*iter == MINUS){
					iter++;
				}
				// add instruction prefix
				add_u8_to_pointer(programpointer, 0x80);
				add_u8_to_pointer(programpointer, 0x2b);
				// number to subtract. (must be byte as cell size is 1 byte)
				add_u8_to_pointer(programpointer, (uint8_t)((iter-count) & 0xff));

				break;

			case LOOP_START:
				if(*(iter + 1) == '-' && *(iter + 2) == ']'){
					iter += 3;
					int count = 0;

					while(*iter == PLUS){
						iter++;
						count++;
					}

					add_u8_to_pointer(programpointer, 0xc6);
					add_u8_to_pointer(programpointer, 0x03);
					add_u8_to_pointer(programpointer, (uint8_t)count);
					break;
				}

				char * tmpiter = iter+1;
				if(*(tmpiter++) == MINUS){
					if(*tmpiter == RIGHT){
						while(*tmpiter == RIGHT){
							tmpiter++;
						}

						if(*(tmpiter++) == PLUS){
							int count = 0;
							while(*tmpiter == LEFT){
								count++;
								tmpiter++;
							}

							if(*(tmpiter) == LOOP_END){
								// mov al, byte[ebx]
								add_u8_to_pointer(programpointer, 0x8a);
								add_u8_to_pointer(programpointer, 0x03);

								// mov byte[ebx], 0
								add_u8_to_pointer(programpointer, 0xc6);
								add_u8_to_pointer(programpointer, 0x03);
								add_u8_to_pointer(programpointer, 0x00);

								// add add byte[rbx+count], al
								add_u8_to_pointer(programpointer, 0x00);
								add_u8_to_pointer(programpointer, 0x83);
								add_u32_to_pointer(programpointer, (uint32_t)count);
								iter = tmpiter+1;
								break;
							}
						}
					}else if(*tmpiter == LEFT){
						while(*tmpiter == LEFT){
							tmpiter++;
						}

						if(*(tmpiter++) == PLUS){
							int count = 0;
							while(*tmpiter == RIGHT){
								count++;
								tmpiter++;
							}

							if(*(tmpiter) == LOOP_END){
								// mov al, byte[ebx]
								add_u8_to_pointer(programpointer, 0x8a);
								add_u8_to_pointer(programpointer, 0x03);

								// mov byte[ebx], 0
								add_u8_to_pointer(programpointer, 0xc6);
								add_u8_to_pointer(programpointer, 0x03);
								add_u8_to_pointer(programpointer, 0x00);

								count = -count;

								// add add byte[rbx+count], al
								add_u8_to_pointer(programpointer, 0x00);
								add_u8_to_pointer(programpointer, 0x83);
								add_u32_to_pointer(programpointer, (uint32_t)count);
								iter = tmpiter+1;
								break;
							}
						}
					}
				}

				// cmp to 0
				add_u8_to_pointer(programpointer, 0x80);
				add_u8_to_pointer(programpointer, 0x3b);
				add_u8_to_pointer(programpointer, 0x00);

				// jmp if equal
				add_u8_to_pointer(programpointer, 0x0f);
				add_u8_to_pointer(programpointer, 0x84);

				// save this position to the stack so the actual address can be written here later
				loopstack[loopstackpointer++] = programpointer;

				// leave 32 bits free for an address
				add_u32_to_pointer(programpointer, 0x00000000);

				iter++;
				break;
			case LOOP_END:
				if(loopstackpointer <= 0){
					printf("Empty loopstack.");
					exit(-1);
				}

				// get the place in the program where the loop start code was written
				uint8_t * loopstart = loopstack[--loopstackpointer]; 
				// difference with some magic offset numbers (instruction lengths)
				uint32_t difference = programpointer - loopstart + 1;
				uint32_t negdifference = -(programpointer - loopstart)-10;

				// set the jump address. Don't subtract the 6 as we want to jump over the next
				// instruction which is conveniently also a 6 byte jump instruction
				add_u32_to_pointer(loopstart, difference);

				// jump back.
				add_u8_to_pointer(programpointer, 0xe9);
				add_u32_to_pointer(programpointer, negdifference);

				iter++;
				break;
			case DISPLAY:
				add_u8_to_pointer(programpointer, 0x8a);
				add_u8_to_pointer(programpointer, 0x13);
				add_u8_to_pointer(programpointer, 0x88);
				add_u8_to_pointer(programpointer, 0x11);
				add_u8_to_pointer(programpointer, 0x48);
				add_u8_to_pointer(programpointer, 0xff);
				add_u8_to_pointer(programpointer, 0xc1);

				iter++;
				break;
			default:
				iter++;
				break; //non instructions are ignored
		}
	}


	add_u8_to_pointer(programpointer, 0xc6);
	add_u8_to_pointer(programpointer, 0x01);
	add_u8_to_pointer(programpointer, 0x00);


	// add return
	add_u8_to_pointer(programpointer, 0xc3);

	#if !FAST
	// set the first byte to an actual nop instruction
	add_u8_to_pointer(program, 0x90);
	#endif
}

void sleep(unsigned long nsec) {
    struct timespec delay = { nsec / 1000000000, nsec % 1000000000 };
    pselect(0, NULL, NULL, NULL, &delay, NULL);
}

void * run(void * program){

	// allocate a tape
	uint8_t * tape = calloc(TAPE_SIZE, sizeof(uint8_t));
	if(tape == NULL){
		printf("couldn't allocate tape buffer\n");
		exit(-1);
	}
	register uint64_t tapepointer = (uint64_t)(tape + (TAPE_SIZE/2));

	uint8_t * iobuffer = calloc(IO_SIZE, sizeof(uint8_t));
	if(iobuffer == NULL){
		printf("couldn't allocate IO buffer\n");
		exit(-1);
	}
	register uint64_t iopointer = (uint64_t)iobuffer;

	#if FAST
	// wait for the compiler to have written <NUMBYTES> bytes
	while(((uint8_t *)program)[NUMBYTES] == 0){
		sleep(200);
	}
	#else
	// wait for the compiler to write it's first byte
	while(((uint8_t *)program)[0] == 0){
		sleep(20);
	}
	#endif

	// run the buffer
	asm volatile(
		"mov %0, %%rax\n"
		"mov %1, %%rbx\n"
		"mov %2, %%rcx\n"
		"callq *%%rax\n"

		:
			"+r"(program),
			"+r"(tapepointer),
			"+r"(iopointer)
		:
		: 
			"rax", // GP
			"rbx", // tape pointer
			"rcx"  // io buffer pointer
	);

	printf("%s",iobuffer);

	return NULL;
}

int main(int argc, char ** argv){
	// make STDOUT asynchronous
	fcntl(1, F_SETFL, O_NONBLOCK);

	// check if there is a filename as second argument
	if(argc < 2){
		printf("no filename found\n");
		exit(-1);
	}

	// get the filename
	char * filename = argv[1];
	printf("running %s\n", filename);

	// allocate an executable buffer
	uint8_t * program = executable_buffer(CODE_BUFFER_SIZE);
	if(program == NULL){
		printf("couldn't allocate executable buffer\n");
		exit(-1);
	}

	// immediately start spawning a new thread cause it is slower than the compiler itself.
	pthread_t thread;
	int err = pthread_create(&thread, NULL, run, program);

	if (err){
		printf("Couldn't spawn thread\n");
		exit(-1);
	}

	// compile the brainfuck to machine code
	compile(program,filename);


	pthread_join(thread, NULL);


	return 0;
}
