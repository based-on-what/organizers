#!/usr/bin/env python3
"""
Enhanced TV Series Duration Analyzer

This script calculates the total duration of TV series by analyzing video files
in subdirectories, providing organized results by series.
"""

import sys
from pathlib import Path
from typing import Dict

try:
    from moviepy.editor import VideoFileClip
except ImportError:
    print("Error: moviepy is required. Install with: pip install moviepy")
    sys.exit(1)

from shared_utils import (
    setup_logging, format_duration, find_files_by_extensions, 
    save_results_to_file, ProgressReporter
)

def get_video_duration(video_file: Path) -> float:
    """
    Get the duration of a video file in seconds.
    
    Args:
        video_file: Path to the video file
        
    Returns:
        Duration in seconds, or 0 if there's an error
    """
    try:
        with VideoFileClip(str(video_file)) as clip:
            return clip.duration or 0
    except Exception as e:
        logging.warning(f"Error processing file {video_file}: {e}")
        return 0


def get_series_durations(
    base_directory: Path, 
    extensions: set = None, 
    excluded_dirs: set = None
) -> Dict[str, float]:
    """
    Calculate total duration of videos grouped by directory (TV series).

    Args:
        base_directory: Path to the base directory
        extensions: Video file extensions to process
        excluded_dirs: Directory names to ignore

    Returns:
        Dictionary mapping directory names to total durations in seconds
    """
    if extensions is None:
        extensions = {'.mp4', '.avi', '.mkv', '.mov', '.wmv', '.flv', '.webm'}

    if excluded_dirs is None:
        excluded_dirs = {"Sub", "Subs", "Subtitles", "Featurettes", "Extras"}

    series_durations = {}
    
    # Process each subdirectory as a series
    for item in base_directory.iterdir():
        if item.is_dir() and item.name not in excluded_dirs:
            # Find all video files in this series directory
            video_files = find_files_by_extensions(
                item, 
                extensions, 
                exclude_dirs=excluded_dirs, 
                recursive=True
            )
            
            if video_files:
                total_duration = 0
                progress = ProgressReporter(len(video_files), f"Processing {item.name}")
                
                for video_file in video_files:
                    duration = get_video_duration(video_file)
                    total_duration += duration
                    progress.update()
                
                progress.finish()
                series_durations[item.name] = total_duration
                logging.info(f"📺 {item.name}: {len(video_files)} files, {format_duration(total_duration)}")
            else:
                logging.info(f"📁 {item.name}: No video files found")
                series_durations[item.name] = 0

    return series_durations


def save_durations_to_file(durations: Dict[str, float], output_file: str) -> None:
    """Save the durations by series to a text file."""
    results_text = []
    for series_name, duration in durations.items():
        formatted_duration = format_duration(duration)
        results_text.append(f"{series_name}: {formatted_duration}")
    
    save_results_to_file(results_text, Path(output_file), "TV SERIES DURATION ANALYSIS")


def main() -> None:
    """Main function to execute the series duration analysis."""
    # Set up logging
    logger = setup_logging("INFO")
    
    base_directory = Path.cwd()  # Get current working directory
    
    # Get durations grouped by directory (series)
    logging.info("Processing video files in series directories...")
    durations = get_series_durations(base_directory)
    
    if not durations:
        logging.warning("No series directories with video files found!")
        return
    
    # Sort durations by value (shortest series first)
    sorted_durations = dict(sorted(durations.items(), key=lambda x: x[1]))
    
    # Display results in console
    logging.info("\nSeries sorted by total duration:")
    total_duration = 0
    for series_name, duration in sorted_durations.items():
        formatted = format_duration(duration)
        logging.info(f"{series_name}: {formatted}")
        total_duration += duration
    
    logging.info(f"\nOverall Statistics:")
    logging.info(f"Total series: {len(durations)}")
    logging.info(f"Total duration: {format_duration(total_duration)}")
    
    # Save results to file
    output_file = "series_durations.txt"
    save_durations_to_file(sorted_durations, output_file)
    logging.info(f"Results saved to {output_file}")


if __name__ == "__main__":
    main()
