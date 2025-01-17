#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
// https://github.com/mborgerding/kissfft
#include "kissfft/kiss_fft.h"
#include <inttypes.h>
#include "constants.h"

/*** Prototypes ***/

/* These 'do' functions should somewhat correspond to tasks in FreeRTOS. */

// Create cancelling noise. 
int do_cancel();

// This function does a fourier on a created signal followed by an inverse
// fourier to get the original signal again.
int do_correctly_working_fft_ifft(void);

// Fourier testing
int do_fourier_test(void);

// Perform recognize functionality. Previously cutoff.c
int do_recognize(void);

// Output anti-noise to speaker.
int do_output_to_speaker(void);

// Use epsilon to compare floating point numbers.
// epsilon specifies how small of a difference can be ignored. i.e. if
// i = 0.0001
// j = 0.0002
// epsilon = 0.001
// then i and j equal each other.
void epsilon_compare_signals(const kiss_fft_cpx *s1, const kiss_fft_cpx *s2,
        const int n, const double epsilon);

// The comparison. Returns TRUE if d1 and d2 are equal. FALSE if they are not.
int epsilon_compare(const double d1, const double d2, const double epsilon);

// Print a complex signal.
void cx_print_signal(const kiss_fft_cpx *s, const int n);

// Print a real signal.
void print_signal(const int16_t *s, const int n);

// Do inverse Fourier transform and restore the original signal from a fourier
// transformed signal.
int ifft_and_restore(const kiss_fft_cfg* state, const kiss_fft_cpx *in,
        kiss_fft_cpx *out, const int n);

// Create a signal and put it in the argument kissfft complex array.
void create_signal(kiss_fft_cpx* cx_in, const int n);

int recognizeEnd(int start, unsigned long startMedium);

// Get the signal and put it in the argument kissfft complex array.
// cx_in must have enough space to hold a segment.
// the segment is retrieved from data_array.
int get_noise_segment(kiss_fft_cpx* cx_in, const int start_noise, const int
        end_noise);

// Invert the frequencies of a complex numbered fourier transformed signal.
int invert_all_frequencies(kiss_fft_cpx* cx_in, const int num_segment_samples);

// Set a frequency of a complex numbered fourier transformed signal to value.
int set_frequency(kiss_fft_cpx* cx_in, const int idx, const double rvalue,
        const double ivalue);

// Set all frequencies of a complex numbered fourier transformed signal to value.
int set_all_frequencies(kiss_fft_cpx* cx_in, const int num_segment_samples, const
        double rvalue, double ivalue);

// Print start and end indices of noises.
void print_noise_indices();

// Print a noise segment
int print_segment(const kiss_fft_cpx* s, const int n);

#if USE_MALLOC
int print_segments(kiss_fft_cpx** segments, const int num_segments,
        const int* sizes);
#else
int print_segments(const kiss_fft_cpx segments[MAX_NSEGMENTS][MAX_NSAMPLES],
        const int num_segments, const int* sizes);
#endif

// Allocates a 2-dimensional array on the heap.
void** malloc2d(const int nrows, const int ncols , const size_t size);

// Make a complex numbered array zero .
void cx_make_zero(kiss_fft_cpx* cx_in, const int size);

void make_zero2d(kiss_fft_cpx** cx_in, const int rows, const int cols);

int free_global_resources();

// Copy data_array and replace noise segments with the cancelling noise
// segments data in the copy. This is for testing purposes as this is clearly
// not realtime.
int copy_signal_and_write_segments_to_copied_signal(int16_t* new_data_array);

// Copy data array into a new array. the new array must be large enough to hold
// all samples of the original data array.
int copy_signal(int16_t* new_data_array, const int original_size);

int print_values_between(const int16_t *s, const int beg, const int end);

int cx_print_values_between(const kiss_fft_cpx *s, const int beg, const int end);

int write_signal_to_file(const char* filename, const int16_t* s, const int n);

/* Tokenize a string. Save tokens in 'tokens' */
/* Returns the number of tokens created. */
int tokenize(char* str, char** tokens, char* delim);

/* Used to fill data_array. Also sets data_array_size. */
int read_data_from_file(int16_t* data_array, const int n, char* filename);



void print_tokens(char** tokens, int n);
void print_ints(int16_t* a, int n);

/* Return index of highest absolute real frequency. */
size_t highest_frequency_real(const kiss_fft_cpx *s, const size_t size);
/* Return index of highest absolute imaginary frequency. */
size_t highest_frequency_imag(const kiss_fft_cpx *s, const size_t size);
/* Cancel x% around the highest absolute frequency in a complex numbered
 * fourier transformed signal.*/
int cancel_interval(kiss_fft_cpx *s, const size_t size, double percent);

/*** Global variables ***/

// Array for the number of samples of each noise segment.
int segment_sizes[MAX_NSEGMENTS];

unsigned int LOOP_SIZE = 120 * 8;
unsigned int INITIAL_LOOP_SIZE = 120 * 8;
// Arrays containing the start index and the end index of the noises.
int start_noise[MAX_NSEGMENTS];
int end_noise[MAX_NSEGMENTS];

// Actual number of noise segments.
int num_noise_segments = 0;

// Segments of anti-noise that will be output to the speaker.
#if USE_MALLOC
kiss_fft_cpx** cx_cancelling_segments;
#else
kiss_fft_cpx cx_cancelling_segments[MAX_NSEGMENTS][MAX_NSAMPLES];
#endif

// The program
int main(void) {
    read_data_from_file(data_array, MAX_DATA_ARRAY_SIZE, FILE_DATA_ARRAY);

    for (int i = 0; ; ++i) {
        int r;
        r = do_recognize();
        /* print_noise_indices(); */
        if (r != OK) return EXIT_FAILURE;
        r = do_cancel();
        if (r != OK) return EXIT_FAILURE;
        /* r = do_output_to_speaker(); */
        /* if (r != OK) return EXIT_FAILURE; */

        int16_t new_data_array[data_array_size];
        copy_signal_and_write_segments_to_copied_signal(new_data_array);

        
        /* print_signal(new_data_array, data_array_size); */
        write_signal_to_file(FILE_NEW_DATA_ARRAY, new_data_array,
                data_array_size);

#if USE_MALLOC
        free_global_resources();
#endif
        // Remove this to run indefinitely.
        return EXIT_SUCCESS;
    }

    /* return EXIT_SUCCESS; */
}


/*** Function definitions ***/

int do_cancel() {
    int r;
    if (num_noise_segments <= 0) {
        fprintf(stderr, "do_cancel: num_noise_segments is less than "
                "or equal to zero.\n");
        return NOT_OK;
    } else if (num_noise_segments >= MAX_NSEGMENTS) {
        fprintf(stderr, "do_cancel: cannot handle this many noise segments.\n");
        return NOT_OK;
    }

    // These will contain the cancelling noise segments that will be output to
    // the speaker.
    // Simply speaking this is an array of signals (an individual signal is an
    // array too, thus we have got a 2-dimensional array).

#if USE_MALLOC
    cx_cancelling_segments = (kiss_fft_cpx**)
        malloc2d(num_noise_segments, MAX_NSAMPLES, sizeof(kiss_fft_cpx));
    make_zero2d(cx_cancelling_segments, num_noise_segments, MAX_NSAMPLES);
#endif

    // kissfft state buffers.
    kiss_fft_cfg kfft_fourier_state;
    kiss_fft_cfg kfft_inverse_fourier_state;

    // For all noise segments:
    //
    // 1. Get a noise segment.
    // 2. Compute Fourier to get the noise frequencies.
    // 3. Invert the frequencies or set frequencies to specified values.
    // 4. Compute inverse fourier to generate cancelling noise.
    for (int i = 0; i < num_noise_segments; ++i) {
        // Compute the length of the segment.
        segment_sizes[i] = end_noise[i] - start_noise[i];

        // Initialise the kissfft state buffers.
        kfft_fourier_state = kiss_fft_alloc(segment_sizes[i], FOURIER, 0, 0);
        kfft_inverse_fourier_state = kiss_fft_alloc(segment_sizes[i],
                INVERSE_FOURIER, 0, 0);

        if (segment_sizes[i] > MAX_NSAMPLES) {
            fprintf(stderr, "do_cancel: I cannot fit it in it is too big!\n");
            goto fail;
        }

        // 1. Get a noise segment.
        kiss_fft_cpx cx_noise_segment[segment_sizes[i]];
        cx_make_zero(cx_noise_segment, segment_sizes[i]);
        r = get_noise_segment(cx_noise_segment, start_noise[i], end_noise[i]);
        if (r != OK) {
            fprintf(stderr, "do_cancel: error while getting noise segment.\n");
            goto fail;
        }

        // 2. Compute Fourier to get the noise frequencies.
        kiss_fft_cpx cx_noise_segment_fourier[segment_sizes[i]];
        cx_make_zero(cx_noise_segment_fourier, segment_sizes[i]);
        kiss_fft(kfft_fourier_state, cx_noise_segment, cx_noise_segment_fourier);

        /* // 3. Invert all frequencies. */
        /* r = invert_all_frequencies(cx_noise_segment_fourier, segment_sizes[i]); */
        /* if (r != OK) { */
        /*     fprintf(stderr, "do_cancel: error while inverting frequencies\n"); */
        /*     goto fail; */
        /* } */

        /* printf("noise segment:\t%d\n", i); */
        /* printf("size of the segment:\t%d\n", segment_sizes[i]); */
        /* cx_print_values_between(cx_noise_segment_fourier, 0, 3000); */

        /* // 3. Set all frequencies to specified rvalue and ivalue. */
        /* r = set_all_frequencies(cx_noise_segment_fourier, segment_sizes[i], 0.0, */
        /*         0.0); */
        /* if (r != OK) { */
        /*     fprintf(stderr, "do_cancel: error while setting frequencies\n"); */
        /*     goto fail; */
        /* } */

        // 3. Perform algorithm to cancel only the highest absolute frequencies
        // of the fourier transformed signal.
        cancel_interval(cx_noise_segment_fourier, segment_sizes[i],
                FREQ_CANCELLATION_PERCENTAGE);
        if (r != OK) {
            fprintf(stderr, "do_cancel: error while cancelling around an"
                    " interval.\n");
            goto fail;
        }

        // 4. Compute inverse fourier to generate cancelling noise.
        if (cx_cancelling_segments[i] == NULL) {
            fprintf(stderr, "do_cancel: cx_cancelling_segments[%d] is NULL\n",
                    i);
            goto fail;
        }
        r = ifft_and_restore(&kfft_inverse_fourier_state,
                cx_noise_segment_fourier, cx_cancelling_segments[i],
                segment_sizes[i]);
        if (r != OK) {
            fprintf(stderr, "do_cancel: error while executing inverse fft.\n");
            goto fail;
        }

        // Free configs.
        free(kfft_fourier_state);
        free(kfft_inverse_fourier_state);
        kfft_fourier_state = NULL;
        kfft_inverse_fourier_state = NULL;
    }

    printf("\n##### DONE with do_cancel! #####\n");
    return OK;

fail:
    free(kfft_fourier_state);
    free(kfft_inverse_fourier_state);
    kfft_fourier_state = NULL;
    kfft_inverse_fourier_state = NULL;
#if USE_MALLOC
    free_global_resources();
#endif
    return NOT_OK;
}

int do_correctly_working_fft_ifft(void) {
    // The kiss_fft config.
    kiss_fft_cfg kfft_state;
    // kiss_fft's complex number type.
    kiss_fft_cpx cx_in[NSAMPLES];
    kiss_fft_cpx cx_out[NSAMPLES];
    kiss_fft_cpx cx_iout[NSAMPLES];

    // Initialise the fft's state buffer.
    // kfft_state is used internally by the fft function.
    // Elaboration on params:
    // NSAMPLES: Is this the number of samples?
    // FOURIER:    Do not use ifft.
    // 0:          Do not place the kfft_state in mem.
    // 0:          No length specify for mem, since we do not use it.
    kfft_state = kiss_fft_alloc(NSAMPLES, FOURIER, 0, 0);

    // Create signal
    create_signal(cx_in, NSAMPLES);

    // Perform fast fourier transform on a complex input buffer.
    // Elaboration on params:
    // kfft_state: The config used internally by the fft.
    // cx_in: input buffer which is an complex numbers array.
    // cx_out: output buffer which is an complex numbers array.
    kiss_fft(kfft_state, cx_in, cx_out);

    // Compare original signal with fourier transform.
    /* printf("Comparing cx_in and cx_out\n"); */
    /* epsilon_compare_signals(cx_in, cx_out, NSAMPLES, 0.0001); */

    /* intensity = sqrt(pow(cx_out[i].r,2) + pow(cx_out[i].i,2)); */
    /* printf("%d - %9.4f\n", i, intensity); */

    // Free data used for config.
    //kiss_fft_free(kfft_state);
    // According to comments in kiss_fft.c regular free should work just fine.
    free(kfft_state);
    kfft_state = NULL;

    // Allocate inverse fourier state.
    kfft_state = kiss_fft_alloc(NSAMPLES, INVERSE_FOURIER, 0, 0);
    ifft_and_restore(&kfft_state, cx_out, cx_iout, NSAMPLES);

    // Compare fourier transformed signal with inverse fourier transformed
    // signal.
    /* printf("Comparing cx_in and cx_iout\n"); */
    /* epsilon_compare_signals(cx_in, cx_iout, 0.0001); */

    printf("\ncx_in\n");
    cx_print_signal(cx_in, NSAMPLES);
    printf("\ncx_out\n");
    cx_print_signal(cx_out, NSAMPLES);
    printf("\ncx_iout\n");
    cx_print_signal(cx_iout, NSAMPLES);
    /* epsilon_compare_signals(cx_in, cx_iout, NSAMPLES, 0.0001); */

    free(kfft_state);
    kfft_state = NULL;

    printf("\nDONE with do_correctly_working_fft_ifft!\n");
    return OK;
}

int do_fourier_test(void) {
    const int samples = 8;
    kiss_fft_cfg kfft_state;

    kiss_fft_cpx cx_in[samples];
    kiss_fft_cpx cx_out[samples];
    kiss_fft_cpx cx_iout[samples];
    cx_make_zero(cx_in, samples);
    cx_make_zero(cx_out, samples);
    cx_make_zero(cx_iout, samples);

    kfft_state = kiss_fft_alloc(samples, FOURIER, 0, 0);

    // Get the first of data_array.
    for (int i = 0; i < samples; ++i) {
        cx_in[i].r = data_array[i];
    }

    // Fourier transform.
    kiss_fft(kfft_state, cx_in, cx_out);

    free(kfft_state);
    kfft_state = NULL;
    kfft_state = kiss_fft_alloc(samples, INVERSE_FOURIER, 0, 0);

    // Inverse Fourier transform.
    ifft_and_restore(&kfft_state, cx_out, cx_iout, samples);
    /* kiss_fft(kfft_state, cx_out, cx_iout); */

    /* printf("\ncx_in\n"); */
    /* print_signal(cx_in, samples); */
    /* printf("\ncx_out\n"); */
    /* print_signal(cx_out, samples); */
    /* printf("\ncx_iout\n"); */
    /* print_signal(cx_iout, samples); */
    epsilon_compare_signals(cx_in, cx_iout, samples, 0.1);
    return OK;
}

void epsilon_compare_signals(const kiss_fft_cpx *s1, const kiss_fft_cpx *s2,
        const int n, const double epsilon) {
    int there_is_a_difference = 0;

    for (int i = 0; i < n; ++i) {
        // Compare real and imaginary part of the signals.
        if (!epsilon_compare(s1[i].r, s2[i].r, epsilon)) {
            there_is_a_difference = 1;
            printf("s1[%d].r = %-15f s2[%d].r = %f\tRe differs.\n", i,
                    s1[i].r, i, s2[i].r);
        }
        if (!epsilon_compare(s1[i].i, s2[i].i, epsilon)) {
            there_is_a_difference = 1;
            printf("s1[%d].i = %-15f s2[%d].i = %f\tIm differs.\n",
                    i, s1[i].i, i, s2[i].i);
        }
    }
    if (there_is_a_difference) {
        putchar('\n');
    } else {
        printf("No differences between the signals.\n");
    }
}

int epsilon_compare(const double d1, const double d2, const double epsilon) {
    if (fabs(d1 - d2) < epsilon) {  // They are equal.
        return TRUE;
    }
    return FALSE;   // They are not equal.
}

void cx_print_signal(const kiss_fft_cpx *s, const int n) {
    for (int i = 0; i < n; ++i) {
        printf("s[%d].r = %f\ts[%d].i = %f\n", i, s[i].r, i, s[i].i);
    }
}

void print_signal(const int16_t *s, const int n) {
    for (int i = 0; i < n; ++i) {
        printf("s[%d] = %-15d\n", i, s[i]);
    }
}

int ifft_and_restore(const kiss_fft_cfg* state, const kiss_fft_cpx *in,
        kiss_fft_cpx *out, const int n) {
    // Perform the kiss fft inverse fourier transform.
    kiss_fft(*state, in, out);
    // We have to divide by n to actually restore the original signal, as
    // the numbers at every index of the signal are multiplied by n. I am
    // not sure why the numbers are multiplied.
    for (int i = 0; i < n; ++i) {
        out[i].r /= n;
        out[i].i /= n;
    }
    return OK;
}

void create_signal(kiss_fft_cpx* cx_in, const int n) {
    for (int i = 0; i < n; ++i) {
        // Fill the real part.
        // I did not want to put only 1's in so I made this if statement up.
        /* cx_in[i].r = 0.0; */
        /* if (i % 2 == 0) { */
        /*     /1* cx_in[i].r = 0.0; *1/ */
        /*     cx_in[i].r = (double) i; */
        /* } */
        cx_in[i].r = (double) i + -4324.324;
        // No imaginary part.
        /* cx_in[i].i = 0.0; */
        cx_in[i].i = (double) i + 52239.23423;
    }

    /* // Conjugate symmetry of real signal. */
    /* for (int i = 0; i < NSAMPLES/2; ++i) { */
    /*     cx_in[NSAMPLES-i].r = cx_in[i].r; */
    /*     cx_in[NSAMPLES-i].i = - cx_in[i].i; */
    /* } */
}

int recognizeEnd(int start, unsigned long startMedium) {
    unsigned long average = 0;
    unsigned long counter = 0;
    // max noise length is the start moment plus 0.5 second.
    unsigned long maxCount = start + MAX_NSAMPLES;
    unsigned long maxLoop = 0;

    // If the max noise length exeeds the arraysize, use arraysize - safezone as
    // max noise length. Safezone is used because next 30 elements will be
    // looped.
    if(maxCount < data_array_size) {
        maxLoop = maxCount;
    } else {
        maxLoop = data_array_size - LOOP_SIZE;
    }
    /* printf("start noise %d", start); */
    // Loop from start of noise to max 0.5 second further.
    for(int k = start; k < maxLoop; k++) {
        counter = 0;
        //printf("rec %d avg: %d point: %d\n", startMedium, average, ((k - start)%LOOP_SIZE));

        // Check if noise drasticly decreases in next 0.05 seconds 
        for(int i = k; i < (k + LOOP_SIZE); i++) {
            counter += abs(data_array[i]);
        }
        if((k - start)%LOOP_SIZE == 0 && k != 0) {
            
            average = counter / LOOP_SIZE;
            counter = 0;
            int safeZone = average / 2;
            // If average is lower than the value at the start of the noise, noise
            // ended so function is stopped.
             printf("inend %d, temp: %d, avg: %d, safe: %d \n", k, startMedium, average, safeZone);
            if(average <= (startMedium + safeZone)) {
                printf(" end noise %d \n", k);
                return k;
            }
        }

    }
    //printf("MAXEND \n");
    //No end of noise is detected, detected noise get discarded.
    /* printf("end noise \n"); */
    return maxLoop;
}

int do_recognize(void) {
    unsigned long counter = 0;
    unsigned long average = 0;
    unsigned long prevAverage = 0;
    unsigned int used = 0;
    int32_t sampleRate = data_array[0];
    unsigned int loopTimes = sampleRate / 48000;
    double safeDivide = 2;
    int decreaseCount = 80;
    int minLoopSize = 120;
    // printf("data_array[683] = %f\n", data_array[683]);
    // Loop complete array, safezone of 400 because next 400 elements are looped
    // before check is reached
    num_noise_segments = 0;

    
    printf("%d \n", data_array[0]);
    LOOP_SIZE = data_array[0];
    INITIAL_LOOP_SIZE = data_array[0];
    decreaseCount = INITIAL_LOOP_SIZE / 12;
    minLoopSize = INITIAL_LOOP_SIZE / 8;
    for (int k = 1; k < (data_array_size - LOOP_SIZE); k += LOOP_SIZE) {
        counter = 0;
        used = 0;
        // Loop next 400 ellements calculate average
        for (int i = k; i < (k + LOOP_SIZE); i++) {
            int data = abs(data_array[i]);
            // If value of data < 500 it can't be heard and is not usable
            if (data > 500) {
                counter += data;
                used++;
            }
        }
        // Save avarage of last loop
        prevAverage = average;
        // If used == 0 don't divide by 0, otherwise divide the counter by the
        // amount of time the counter is edited.
        if (used != 0) {
            average = counter / used;
        }
        else {
            average = 0;
        }
        if (k != 0) {
            //Check if an huge increase in volume occured.
            unsigned long safeZone = (prevAverage / safeDivide);
            unsigned long tempAverage = prevAverage + safeZone;
            printf("%d, temp: %d, avg: %d, safe: %d \n", k, tempAverage, average, safeZone);
            if (tempAverage < average) {

                /* *(start_noise + num_noise_segments) = k; */
                start_noise[num_noise_segments] = k;
                k = recognizeEnd(k, prevAverage - safeZone);
                if(LOOP_SIZE < INITIAL_LOOP_SIZE) {
                    LOOP_SIZE = INITIAL_LOOP_SIZE;
                }
                /* *(end_noise + num_noise_segments) = k; */
                end_noise[num_noise_segments] = k;
                num_noise_segments++;
            } else if(LOOP_SIZE > minLoopSize) {
                //printf("%d \n", safeDivide);*

                LOOP_SIZE -= decreaseCount;
            }
        }
    }

    printf("\n##### DONE with do_recognize! #####\n");

    return OK;
}

int do_output_to_speaker(void) {
    int r;
    r = print_segments(cx_cancelling_segments, num_noise_segments,
            segment_sizes);
    return r;
}

void print_noise_indices() {
    printf("\n############");
    printf(" Start and end indices of noises.");
    printf(" ############\n");
    for (int i = 0; i < num_noise_segments; ++i) {
        printf("start_noise[%d]:\t%d\n", i, start_noise[i]);
        printf("end_noise[%d]:\t%d\n", i, end_noise[i]);
        printf("size:\t\t%d\n", end_noise[i]-start_noise[i]);
    }
    printf("############\n\n");
}

int get_noise_segment(kiss_fft_cpx* cx_in, const int start_noise, const int
        end_noise) {
    const int num_segment_samples = end_noise - start_noise;
    // Ensure array bounds are correct.
    if (start_noise < 0 || end_noise > data_array_size ||
            num_segment_samples > data_array_size || num_segment_samples < 0) {
        fprintf(stderr, "get_noise_segment: error in segment bounds.\n");
        return NOT_OK;
    }
    // Put data_array noise segment in the kissfft complex array.
    // We assume cx_in is big enough to contain a noise segment.
    for (int i = 0, j = start_noise; j < end_noise; ++i, ++j) {
        cx_in[i].r = data_array[j];
        cx_in[i].i = 0.0;
    }
    return OK;
}

int set_frequency(kiss_fft_cpx* cx_in, const int idx, const double rvalue,
        const double ivalue) {
    cx_in[idx].r = rvalue;
    cx_in[idx].i = ivalue;
    return OK;
}

int invert_all_frequencies(kiss_fft_cpx* cx_in, const int num_segment_samples) {
    int r;
    for (int i = 0; i < num_segment_samples; ++i) {
        r = set_frequency(cx_in, i, -cx_in[i].r, -cx_in[i].i);
        if (r != OK) return NOT_OK;
    }
    return OK;
}

int set_all_frequencies(kiss_fft_cpx* cx_in, const int num_segment_samples, const
        double rvalue, double ivalue) {
    int r;
    for (int i = 0; i < num_segment_samples; ++i) {
        r = set_frequency(cx_in, i, rvalue, ivalue);
        if (r != OK) return NOT_OK;
    }
    return OK;
}

int print_segment(const kiss_fft_cpx* s, const int n) {
    if (s == NULL) return NOT_OK;
    for (int i = 0; i < n; ++i) {
        printf("s[%d].r = %-15f\ts[%d].i = %-15f\n", i, s[i].r, i, s[i].i);
    }
    return OK;
}

#if USE_MALLOC
int print_segments(kiss_fft_cpx** segments, const int num_segments,
        const int* sizes) {
    if (segments == NULL) return NOT_OK;
    for (int i = 0; i < num_segments; ++i) {
        printf("Segment: %d\n", i);
        if (print_segment(segments[i], sizes[i]) != OK) return NOT_OK;
    }
    return OK;
}
#else
int print_segments(const kiss_fft_cpx segments[MAX_NSEGMENTS][MAX_NSAMPLES],
        const int num_segments, const int* sizes) {
    if (segments == NULL) return NOT_OK;
    for (int i = 0; i < num_segments; ++i) {
        printf("Segment: %d\n", i);
        if (print_segment(segments[i], sizes[i]) != OK) return NOT_OK;
    }
    return OK;
}
#endif

void** malloc2d(const int nrows, const int ncols , const size_t size) {
    void** p;
    p = malloc(sizeof(*p) * nrows);
    if (p == NULL) {
        fprintf(stderr, "malloc_2d_array: error in allocating space.\n");
        return NULL;
    }
    for (int i = 0; i < nrows; ++i) {
        p[i] = malloc(size * ncols);
        if(p[i] == NULL) {
            fprintf(stderr, "malloc_2d_array: error in allocating array.\n");
            free(p);
            p = NULL;
            return NULL;
        }
    }
    return p;
}

void cx_make_zero(kiss_fft_cpx* cx_in, const int size) {
    for (int i = 0; i < size; ++i) {
        cx_in[i].r = 0.0;
        cx_in[i].i = 0.0;
    }
}

void make_zero2d(kiss_fft_cpx** cx_in, const int rows, const int cols) {
    for (int i = 0; i < rows; ++i) cx_make_zero(*cx_in, cols); 
}

int free_global_resources() {
    free(cx_cancelling_segments);
    cx_cancelling_segments = NULL;
    return OK;
}

int copy_signal_and_write_segments_to_copied_signal(int16_t* new_data_array) {
    int r;
    r = copy_signal(new_data_array, data_array_size);
    if (r != OK) return r;

    // Copy segments of anti-noise into the new_data_array.
    for (int i = 0; i < num_noise_segments; ++i) {
        for (int j = start_noise[i], k = 0; j < end_noise[i]; ++j, ++k) {
            // Take the real part only.
            new_data_array[j] = cx_cancelling_segments[i][k].r;
        }
    }

    return OK;
}

int copy_signal(int16_t* new_data_array, const int original_size) {
    for (int i = 0; i < original_size; ++i) {
        new_data_array[i] = data_array[i];
    }
    return OK;
}

int print_values_between(const int16_t *s, const int beg, const int end) {
    printf("Values between '%d' and '%d'\n", beg, end);
    for (int i = beg; i < end; ++i) {
        printf("s[%d] = %-15d\n", i, s[i]);
    }
    return OK;
}

int cx_print_values_between(const kiss_fft_cpx *s, const int beg, const int end) {
    printf("Values between '%d' and '%d'\n", beg, end);
    for (int i = beg; i < end; ++i) {
        printf("s[%d].r = %-15f\ts[%d].i = %-15f\n", i, s[i].r, i, s[i].i);
    }
    return OK;
}

int write_signal_to_file(const char* filename, const int16_t* s, const int n) {
    FILE *f;
    f = fopen(filename, "w");
    for (int i = 0; i < n; ++i) {
        fprintf(f, "%d%s",  s[i], (i == n-1) ? "" : ",");
    }
    printf("##### Wrote to file! #####\n");
    fclose(f);
    return OK;
}

int read_data_from_file(int16_t* data_array, const int n, char* filename) {
    /* Read from text file. */
    FILE *f = fopen(filename, "r");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    char *data = malloc(fsize + 1);
    fread(data, 1, fsize, f);
    fclose(f);

    data[fsize] = 0;

    /* Tokenize */
    char* tokens[MAX_DATA_ARRAY_SIZE];
    int num_tokens = tokenize(data, tokens, ",");
    if (num_tokens > MAX_DATA_ARRAY_SIZE) {
        fprintf(stderr, "read_data_from_file: error, too many tokens");
        return NOT_OK;
    }

    /* Convert tokens to int16. */
    for (int i = 0; i < num_tokens; ++i) {
        data_array[i] = (int16_t) strtol(tokens[i], NULL, 10);
    }

    /* Set the size of the data array. */
    data_array_size = num_tokens;

    /* print_tokens(tokens, num_tokens); */
    /* print_ints(data_array, num_tokens); */
    return 0;
}

int tokenize(char* str, char** tokens, char* delim) {
    int n;
    for (n = 0; (*tokens++ = strtok(str, delim)); ++n, str = NULL);
    return n;
}

void print_tokens(char** tokens, int n) {
    while (n--) printf("%s\n", *tokens++);
}

void print_ints(int16_t* a, int n) {
    for (int i = 0; i < n; ++i) printf("%d,", a[i]);
    putchar('\n');
}

/* Return index of highest absolute real frequency. */
size_t highest_frequency_real(const kiss_fft_cpx *s, const size_t size) {
    size_t current_highest = 0;
    for (int i = 1; i < size; ++i) {
        if (fabs(s[i].r) > fabs(s[current_highest].r)) {
            current_highest = i;
        }
    }
    return current_highest;
}

/* Return index of highest absolute imaginary frequency. */
size_t highest_frequency_imag(const kiss_fft_cpx *s, const size_t size) {
    size_t current_highest = 0;
    for (int i = 1; i < size; ++i) {
        if (fabs(s[i].i) > fabs(s[current_highest].i)) {
            current_highest = i;
        }
    }
    return current_highest;
}

/* Set x% around the absolute highest frequency to zero. */
int cancel_interval(kiss_fft_cpx *s, const size_t size, double percent) {
    size_t interval, hfreq_idx_re, hfreq_idx_im;
    int beg_re, beg_im, end_re, end_im;

    if (percent < 0 || percent > 100) return NOT_OK;
    interval = percent/100.0 * size;
    
    /* Get the index of the highest frequency of the real and imaginary parts.*/
    hfreq_idx_re = highest_frequency_real(s, size);
    hfreq_idx_im = highest_frequency_imag(s, size);

    /* Get begin and end intervals. */
    beg_re = hfreq_idx_re - interval;
    beg_im = hfreq_idx_im - interval;
    end_re = hfreq_idx_re + interval;
    end_im = hfreq_idx_im + interval;

    if (beg_re < 0)    beg_re = 0;
    if (beg_im < 0)    beg_im = 0;
    if (end_re > size) end_re = size;
    if (end_im > size) end_im = size;

    /* /1* Set frequencies to zero *1/ */
    /* for (int i = beg_re; i < end_re; ++i) { */
    /*     s[i].r = 0.0; */
    /* } */
    /* for (int i = beg_im; i < end_im; ++i) { */
    /*     s[i].i = 0.0; */
    /* } */

    /* Invert frequencies */
    for (int i = beg_re; i < end_re; ++i) {
        s[i].r = -s[i].r;
    }
    for (int i = beg_im; i < end_im; ++i) {
        s[i].i = -s[i].i;
    }

    return OK;
}
