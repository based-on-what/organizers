#!/usr/bin/env python3
"""
Enhanced Video Duration Analyzer

This script recursively scans directories for video files and calculates their durations,
providing detailed logging and robust error handling for corrupted files.
"""

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

try:
    from moviepy.editor import VideoFileClip
except ImportError:
    print("Error: moviepy is required. Install with: pip install moviepy")
    sys.exit(1)

from shared_utils import (
    setup_logging, format_duration, format_file_size, safe_file_operation,
    find_files_by_extensions, save_results_to_file, ProgressReporter
)


def get_video_duration(video_path: Path) -> Optional[Tuple[float, int]]:
    """
    Get the duration of a video file and its size.
    
    Args:
        video_path: Path to the video file
        
    Returns:
        Tuple of (duration_in_seconds, file_size_in_bytes) or None if error
    """
    try:
        # Use shared utility for file validation
        if not safe_file_operation(video_path, "video analysis"):
            return None
            
        file_size = video_path.stat().st_size
        
        # Skip files smaller than 100KB (likely corrupted or not actual videos)
        if file_size < 100 * 1024:
            logging.warning(f"File too small ({file_size} bytes): {video_path}")
            return None
        
        # Try to open the video file
        clip = None
        try:
            clip = VideoFileClip(str(video_path))
            duration = clip.duration
            
            if duration is None or duration <= 0:
                logging.warning(f"Invalid duration for file: {video_path}")
                return None
                
            return duration, file_size
            
        except Exception as e:
            logging.error(f"Error opening video file {video_path}: {e}")
            return None
        finally:
            # Ensure clip is properly closed
            if clip is not None:
                try:
                    clip.close()
                except:
                    pass
            
    except Exception as e:
        logging.error(f"Unexpected error processing {video_path}: {e}")
        return None


def analyze_videos_sequential(video_files: List[Path]) -> Dict[str, Dict]:
    """
    Analyze video files sequentially with robust error handling.
    
    Args:
        video_files: List of video file paths
        
    Returns:
        Dictionary with all video information
    """
    if not video_files:
        return {}
    
    results = {}
    progress = ProgressReporter(len(video_files), "Analyzing videos")
    
    logging.info(f"Starting sequential processing of {len(video_files)} files")
    
    for video_path in video_files:
        try:
            result = get_video_duration(video_path)
            if result is not None:
                duration, file_size = result
                results[str(video_path)] = {
                    'duration': duration,
                    'file_size': file_size,
                    'formatted_duration': format_duration(duration),
                    'formatted_size': format_file_size(file_size)
                }
                logging.info(
                    f"✓ {video_path.name}: {format_duration(duration)} | "
                    f"{format_file_size(file_size)}"
                )
            else:
                logging.warning(f"✗ Skipped: {video_path.name} (unable to process)")
                
        except KeyboardInterrupt:
            logging.info("Process interrupted by user")
            break
        except Exception as e:
            logging.error(f"✗ Error processing {video_path.name}: {e}")
        
        progress.update()
    
    progress.finish()
    return results


def save_results(
    results: Dict[str, Dict],
    output_file: Path,
    format_type: str = "txt"
) -> None:
    """
    Save results to file in specified format.
    
    Args:
        results: Dictionary with video information
        output_file: Path to output file
        format_type: Output format ("txt" or "json")
    """
    try:
        if format_type.lower() == "json":
            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)
        else:
            # Sort by duration (shortest first)
            sorted_items = sorted(
                results.items(),
                key=lambda x: x[1]['duration']
            )
            
            # Prepare results for shared utility
            results_text = []
            for file_path, info in sorted_items:
                results_text.extend([
                    f"File: {Path(file_path).name}",
                    f"Path: {file_path}",
                    f"Duration: {info['formatted_duration']}",
                    f"Size: {info['formatted_size']}",
                    "-" * 50
                ])
            
            # Add summary
            total_duration = sum(info['duration'] for info in results.values())
            total_size = sum(info['file_size'] for info in results.values())
            file_count = len(results)
            
            results_text.extend([
                "",
                "SUMMARY:",
                f"Total files: {file_count}",
                f"Total duration: {format_duration(total_duration)}",
                f"Total size: {format_file_size(total_size)}"
            ])
            
            if file_count > 0:
                results_text.extend([
                    f"Average duration: {format_duration(total_duration / file_count)}",
                    f"Average size: {format_file_size(total_size / file_count)}"
                ])
            
            save_results_to_file(results_text, output_file, "VIDEO DURATION ANALYSIS REPORT")
                
    except Exception as e:
        logging.error(f"Error saving results to {output_file}: {e}")


def print_summary(results: Dict[str, Dict]) -> None:
    """Print summary statistics."""
    if not results:
        logging.info("No video files were successfully processed")
        return
    
    total_duration = sum(info['duration'] for info in results.values())
    total_size = sum(info['file_size'] for info in results.values())
    file_count = len(results)
    
    logging.info("=" * 50)
    logging.info("SUMMARY STATISTICS")
    logging.info("=" * 50)
    logging.info(f"Total files processed: {file_count}")
    logging.info(f"Total duration: {format_duration(total_duration)}")
    logging.info(f"Total size: {format_file_size(total_size)}")
    logging.info(f"Average duration: {format_duration(total_duration / file_count)}")
    logging.info(f"Average size: {format_file_size(total_size / file_count)}")


def main():
    """Main function with command-line argument parsing."""
    parser = argparse.ArgumentParser(
        description="Analyze video file durations in directories"
    )
    parser.add_argument(
        "directory",
        nargs="?",
        default=".",
        help="Directory to analyze (default: current directory)"
    )
    parser.add_argument(
        "-o", "--output",
        default="video_duration_analysis.txt",
        help="Output file name (default: video_duration_analysis.txt)"
    )
    parser.add_argument(
        "-f", "--format",
        choices=["txt", "json"],
        default="txt",
        help="Output format (default: txt)"
    )
    parser.add_argument(
        "-e", "--extensions",
        nargs="+",
        default=[".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"],
        help="Video file extensions to include"
    )
    parser.add_argument(
        "-x", "--exclude",
        nargs="+",
        default=["Sub", "Subs", "Subtitles", "Featurettes", "Extras"],
        help="Directory names to exclude"
    )
    parser.add_argument(
        "-l", "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        default="INFO",
        help="Logging level (default: INFO)"
    )
    
    args = parser.parse_args()
    
    # Setup logging
    logger = setup_logging(args.log_level, "video_analyzer.log")
    
    # Convert arguments to appropriate types
    base_path = Path(args.directory).resolve()
    output_file = Path(args.output)
    video_extensions = {ext.lower() for ext in args.extensions}
    excluded_dirs = set(args.exclude)
    
    # Validate input directory
    if not base_path.exists():
        logger.error(f"Directory does not exist: {base_path}")
        sys.exit(1)
    
    if not base_path.is_dir():
        logger.error(f"Path is not a directory: {base_path}")
        sys.exit(1)
    
    logger.info(f"Starting video analysis in: {base_path}")
    logger.info(f"Video extensions: {', '.join(video_extensions)}")
    logger.info(f"Excluded directories: {', '.join(excluded_dirs)}")
    
    start_time = time.time()
    
    # Find video files using shared utility
    logger.info("Scanning for video files...")
    video_files = find_files_by_extensions(
        base_path, 
        video_extensions, 
        exclude_dirs=excluded_dirs, 
        recursive=True
    )
    
    if not video_files:
        logger.warning("No video files found!")
        return
    
    logger.info(f"Found {len(video_files)} video files")
    
    # Process videos sequentially
    results = analyze_videos_sequential(video_files)
    
    # Save results
    if results:
        logger.info(f"Saving results to: {output_file}")
        save_results(results, output_file, args.format)
    else:
        logger.warning("No results to save - all files failed to process")
    
    # Print summary
    processing_time = time.time() - start_time
    logger.info(f"Processing completed in {processing_time:.2f} seconds")
    print_summary(results)


if __name__ == "__main__":
    main()
