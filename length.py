#!/usr/bin/env python3
"""
Enhanced Video Duration Analyzer

This script recursively scans directories for video files and calculates their durations,
providing detailed logging and robust error handling for corrupted files.
"""

import argparse
import json
import logging
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple
import time

try:
    from moviepy.editor import VideoFileClip
except ImportError:
    print("Error: moviepy is required. Install with: pip install moviepy")
    sys.exit(1)


def setup_logging(log_level: str = "INFO") -> logging.Logger:
    """Set up logging configuration with timestamps and proper formatting."""
    # Configure file handler with UTF-8 encoding
    file_handler = logging.FileHandler('video_analyzer.log', encoding='utf-8')
    file_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
    
    # Configure console handler with UTF-8 encoding
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
    
    # Set up logging
    logging.basicConfig(
        level=getattr(logging, log_level.upper()),
        handlers=[file_handler, console_handler]
    )
    return logging.getLogger(__name__)


def get_video_duration(video_path: Path) -> Optional[Tuple[float, int]]:
    """
    Get the duration of a video file and its size.
    
    Args:
        video_path: Path to the video file
        
    Returns:
        Tuple of (duration_in_seconds, file_size_in_bytes) or None if error
    """
    try:
        # Check if file exists and is not empty
        if not video_path.exists():
            logging.warning(f"File does not exist: {video_path}")
            return None
            
        file_size = video_path.stat().st_size
        
        if file_size == 0:
            logging.warning(f"File is empty: {video_path}")
            return None
            
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
            
    except PermissionError:
        logging.error(f"Permission denied: {video_path}")
        return None
    except OSError as e:
        logging.error(f"OS error accessing {video_path}: {e}")
        return None
    except Exception as e:
        logging.error(f"Unexpected error processing {video_path}: {e}")
        return None


def format_duration(seconds: float) -> str:
    """Format duration in seconds to human-readable format."""
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = int(seconds % 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"


def format_file_size(size_bytes: int) -> str:
    """Format file size in bytes to human-readable format."""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.1f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.1f} TB"


def find_video_files(
    base_path: Path,
    video_extensions: Set[str],
    excluded_dirs: Set[str]
) -> List[Path]:
    """
    Find all video files in the directory tree, excluding specified directories.
    
    Args:
        base_path: Base directory to search
        video_extensions: Set of video file extensions to include
        excluded_dirs: Set of directory names to exclude
        
    Returns:
        List of Path objects for video files
    """
    video_files = []
    
    try:
        for root, dirs, files in os.walk(base_path):
            # Remove excluded directories from the search
            dirs[:] = [d for d in dirs if d not in excluded_dirs]
            
            root_path = Path(root)
            for file in files:
                try:
                    file_path = root_path / file
                    if file_path.suffix.lower() in video_extensions:
                        video_files.append(file_path)
                except Exception as e:
                    logging.warning(f"Error accessing file {file}: {e}")
                    continue
    except Exception as e:
        logging.error(f"Error walking directory {base_path}: {e}")
    
    return video_files


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
    processed_count = 0
    error_count = 0
    
    logging.info(f"Starting sequential processing of {len(video_files)} files")
    
    for i, video_path in enumerate(video_files, 1):
        try:
            logging.info(f"Processing file {i}/{len(video_files)}: {video_path.name}")
            
            result = get_video_duration(video_path)
            if result is not None:
                duration, file_size = result
                results[str(video_path)] = {
                    'duration': duration,
                    'file_size': file_size,
                    'formatted_duration': format_duration(duration),
                    'formatted_size': format_file_size(file_size)
                }
                processed_count += 1
                logging.info(
                    f"[OK] Completed: {video_path.name} | "
                    f"Duration: {format_duration(duration)} | "
                    f"Size: {format_file_size(file_size)}"
                )
            else:
                error_count += 1
                logging.warning(f"[SKIP] Skipped: {video_path.name} (unable to process)")
                
        except KeyboardInterrupt:
            logging.info("Process interrupted by user")
            break
        except Exception as e:
            error_count += 1
            logging.error(f"[ERROR] Error processing {video_path.name}: {e}")
            continue
    
    logging.info(f"Processing summary: {processed_count} successful, {error_count} errors")
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
            with open(output_file, 'w', encoding='utf-8') as f:
                # Sort by duration (shortest first)
                sorted_items = sorted(
                    results.items(),
                    key=lambda x: x[1]['duration']
                )
                
                f.write("VIDEO DURATION ANALYSIS REPORT\n")
                f.write("=" * 50 + "\n\n")
                
                for file_path, info in sorted_items:
                    f.write(f"File: {Path(file_path).name}\n")
                    f.write(f"Path: {file_path}\n")
                    f.write(f"Duration: {info['formatted_duration']}\n")
                    f.write(f"Size: {info['formatted_size']}\n")
                    f.write("-" * 50 + "\n")
                
                # Add summary at the end
                total_duration = sum(info['duration'] for info in results.values())
                total_size = sum(info['file_size'] for info in results.values())
                file_count = len(results)
                
                f.write(f"\nSUMMARY:\n")
                f.write(f"Total files: {file_count}\n")
                f.write(f"Total duration: {format_duration(total_duration)}\n")
                f.write(f"Total size: {format_file_size(total_size)}\n")
                if file_count > 0:
                    f.write(f"Average duration: {format_duration(total_duration / file_count)}\n")
                    f.write(f"Average size: {format_file_size(total_size / file_count)}\n")
                
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
    logger = setup_logging(args.log_level)
    
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
    
    # Find video files
    logger.info("Scanning for video files...")
    video_files = find_video_files(base_path, video_extensions, excluded_dirs)
    
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
