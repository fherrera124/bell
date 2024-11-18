import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq
from .dsp_wrapper import dsp_run_pipeline, dsp_parse_pipeline


def generate_noise(sample_rate, duration, pcm_min, pcm_max):
    """
    Generates white noise in PCM format.
    """
    num_samples = int(sample_rate * duration)
    white_noise = np.random.randint(pcm_min, pcm_max + 1, num_samples, dtype=np.int16)
    return white_noise


def process_dsp(input_pcm, output_pcm, num_channels, bit_width, sample_rate, pipeline_json_str):
    """
    Process the PCM data through a DSP pipeline.
    """
    dsp_parse_pipeline(pipeline_json_str)
    
    if dsp_run_pipeline(input_pcm, output_pcm, num_channels, bit_width, sample_rate):
        processed_noise = np.frombuffer(output_pcm, dtype=np.int16)
        return processed_noise
    else:
        raise RuntimeError("DSP pipeline processing failed")


def plot(white_noise, processed_noise, num_samples, sample_rate, duration):
    """
    Plot the time-domain and frequency-domain representations.
    """
    frequencies = fftfreq(num_samples, 1 / sample_rate)
    fft_original = fft(white_noise)
    fft_processed = fft(processed_noise)
    
    plt.figure(figsize=(12, 6))

    # Time-domain plot
    plt.subplot(3, 1, 1)
    time = np.linspace(0, duration, num_samples, endpoint=False)
    plt.plot(time, white_noise, color="blue")
    plt.title("Time-Domain Representation of White Noise")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid()

    # Time-domain plot: Processed
    plt.subplot(3, 1, 2)
    plt.plot(time, processed_noise, color="blue")
    plt.title("Time-Domain Representation of Processed Noise")
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.grid()

    # Frequency-domain plot: Comparison
    plt.subplot(3, 1, 3)
    plt.plot(frequencies[:num_samples // 2], np.abs(fft_original)[:num_samples // 2], color="blue", label="Original")
    plt.plot(frequencies[:num_samples // 2], np.abs(fft_processed)[:num_samples // 2], color="green", label="Processed")
    plt.title("Frequency-Domain Comparison")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.legend()
    plt.grid()

    # Show plots
    plt.tight_layout()
    plt.show()


def main():
    # Parse input arguments
    if len(sys.argv) != 2:
        print("Usage: python plot_pipeline.py [pipeline file json]")
        sys.exit(1)

    filename = sys.argv[1]

    # Read the pipeline JSON from the file
    with open(filename, 'r') as file:
        pipeline_json_str = file.read()

    # Configuration parameters
    SAMPLE_RATE = 44100  # Hz
    DURATION = 1.0  # seconds
    PCM_MAX = 32767  # Max value for uint16_t
    PCM_MIN = -32767  # Min value for uint16_t
    NUM_CHANNELS = 1  # Mono
    BIT_WIDTH = 16  # 16-bit

    # Generate PCM white noise
    white_noise = generate_noise(SAMPLE_RATE, DURATION, PCM_MIN, PCM_MAX)

    # Prepare input and output buffers
    input_pcm = white_noise.tobytes()
    output_pcm = bytearray(len(input_pcm))

    # Process through DSP pipeline
    processed_noise = process_dsp(input_pcm, output_pcm, NUM_CHANNELS, BIT_WIDTH, SAMPLE_RATE, pipeline_json_str)

    # Plot the results
    plot(white_noise, processed_noise, len(white_noise), SAMPLE_RATE, DURATION)


if __name__ == "__main__":
    main()
