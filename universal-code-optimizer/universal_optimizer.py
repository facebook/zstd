#!/usr/bin/env python3
"""
Universal Code Optimizer - Main Entry Point
Extreme optimization system that removes ALL human legibility
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional
import threading
import time

# Add the current directory to Python path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.language_detector import LanguageDetector
from core.symbol_generator import SymbolGenerator
from optimizers.c_optimizer import COptimizer
from server.translation_server import TranslationServer

class UniversalOptimizer:
    """Main optimizer that coordinates all language-specific optimizers"""
    
    def __init__(self, strategy: str = "memory_optimized", server_port: int = 8080):
        self.strategy = strategy
        self.server_port = server_port
        self.detector = LanguageDetector()
        self.optimizers = {}
        self.results = {}
        self.translation_server = None
        
        print("🚀 Universal Code Optimizer")
        print("=" * 50)
        print(f"Strategy: {strategy}")
        print(f"Server Port: {server_port}")
        print()
        
        self._initialize_optimizers()
    
    def _initialize_optimizers(self):
        """Initialize language-specific optimizers"""
        print("🔧 Initializing optimizers...")
        
        # Initialize available optimizers
        self.optimizers = {
            'c': COptimizer(self.strategy),
            'cpp': COptimizer(self.strategy),  # Use C optimizer for C++ for now
            # TODO: Add more language optimizers
            # 'javascript': JavaScriptOptimizer(self.strategy),
            # 'python': PythonOptimizer(self.strategy),
            # 'rust': RustOptimizer(self.strategy),
        }
        
        print(f"✅ Initialized {len(self.optimizers)} optimizers")
    
    def optimize_file(self, file_path: str, output_path: Optional[str] = None) -> Dict:
        """Optimize a single file"""
        print(f"\n📄 Processing: {file_path}")
        
        # Detect language
        language, confidence = self.detector.detect_language(file_path)
        print(f"🔍 Detected language: {language} (confidence: {confidence:.2f})")
        
        if language == 'unknown':
            print("⚠️  Unknown language, skipping optimization")
            return {'error': 'Unknown language', 'file': file_path}
        
        if language not in self.optimizers:
            print(f"⚠️  No optimizer available for {language}")
            return {'error': f'No optimizer for {language}', 'file': file_path}
        
        # Get optimizer and process file
        optimizer = self.optimizers[language]
        
        try:
            result = optimizer.optimize_file(file_path, output_path)
            result['language'] = language
            result['confidence'] = confidence
            
            print(f"✅ Optimized: {result['reduction_percent']:.1f}% reduction")
            return result
            
        except Exception as e:
            print(f"❌ Error optimizing {file_path}: {e}")
            return {'error': str(e), 'file': file_path, 'language': language}
    
    def optimize_directory(self, directory: str, output_directory: str, 
                         recursive: bool = True, file_extensions: Optional[List[str]] = None) -> Dict:
        """Optimize all files in a directory"""
        print(f"\n📁 Processing directory: {directory}")
        print(f"📤 Output directory: {output_directory}")
        
        # Create output directory
        os.makedirs(output_directory, exist_ok=True)
        
        # Analyze directory structure
        analysis = self.detector.analyze_directory(directory)
        
        print(f"\n📊 Directory Analysis:")
        total_files = sum(len(files) for files in analysis.values())
        print(f"Total files: {total_files}")
        
        for language, files in analysis.items():
            if files:
                print(f"  {language}: {len(files)} files")
        
        # Process files by language
        results = {
            'total_files': 0,
            'optimized_files': 0,
            'total_reduction_bytes': 0,
            'total_reduction_percent': 0.0,
            'by_language': {},
            'errors': []
        }
        
        for language, files in analysis.items():
            if language == 'unknown' or language not in self.optimizers:
                continue
            
            print(f"\n🔧 Optimizing {language} files...")
            
            lang_results = {
                'files': 0,
                'reduction_bytes': 0,
                'reduction_percent': 0.0,
                'processed_files': []
            }
            
            optimizer = self.optimizers[language]
            
            for file_info in files:
                file_path = file_info['path']
                rel_path = os.path.relpath(file_path, directory)
                output_path = os.path.join(output_directory, rel_path)
                
                # Create output subdirectory
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                
                try:
                    result = optimizer.optimize_file(file_path, output_path)
                    
                    lang_results['files'] += 1
                    lang_results['reduction_bytes'] += result['reduction_bytes']
                    lang_results['processed_files'].append(result)
                    
                    results['total_files'] += 1
                    results['optimized_files'] += 1
                    results['total_reduction_bytes'] += result['reduction_bytes']
                    
                    print(f"  ✅ {rel_path}: {result['reduction_percent']:.1f}% reduction")
                    
                except Exception as e:
                    error_info = {'file': file_path, 'error': str(e), 'language': language}
                    results['errors'].append(error_info)
                    print(f"  ❌ {rel_path}: {e}")
            
            # Calculate language-specific statistics
            if lang_results['files'] > 0:
                total_original = sum(f['original_size'] for f in lang_results['processed_files'])
                total_optimized = sum(f['optimized_size'] for f in lang_results['processed_files'])
                
                if total_original > 0:
                    lang_results['reduction_percent'] = ((total_original - total_optimized) / total_original) * 100
                
                # Save language-specific dictionary
                dict_path = os.path.join(output_directory, f'{language}_optimization_dictionary.json')
                optimizer.save_dictionary(dict_path)
                lang_results['dictionary_path'] = dict_path
            
            results['by_language'][language] = lang_results
        
        # Calculate overall statistics
        if results['optimized_files'] > 0:
            total_original = 0
            total_optimized = 0
            
            for lang_data in results['by_language'].values():
                for file_data in lang_data['processed_files']:
                    total_original += file_data['original_size']
                    total_optimized += file_data['optimized_size']
            
            if total_original > 0:
                results['total_reduction_percent'] = ((total_original - total_optimized) / total_original) * 100
        
        # Save overall results
        results_path = os.path.join(output_directory, 'optimization_results.json')
        with open(results_path, 'w', encoding='utf-8') as f:
            json.dump(results, f, indent=2)
        
        self._print_summary(results)
        return results
    
    def _print_summary(self, results: Dict):
        """Print optimization summary"""
        print(f"\n🎯 Optimization Summary")
        print("=" * 50)
        print(f"Total files processed: {results['optimized_files']}")
        print(f"Total size reduction: {results['total_reduction_bytes']:,} bytes ({results['total_reduction_percent']:.1f}%)")
        
        if results['errors']:
            print(f"Errors: {len(results['errors'])}")
        
        print(f"\nBy Language:")
        for language, lang_data in results['by_language'].items():
            if lang_data['files'] > 0:
                print(f"  {language.upper()}: {lang_data['files']} files, {lang_data['reduction_percent']:.1f}% reduction")
    
    def start_translation_server(self, background: bool = True):
        """Start the translation server"""
        print(f"\n🌐 Starting Translation Server...")
        
        # Create dictionaries directory
        dict_dir = os.path.join(os.path.dirname(__file__), 'dictionaries')
        os.makedirs(dict_dir, exist_ok=True)
        
        # Copy dictionaries from optimizers
        for language, optimizer in self.optimizers.items():
            if hasattr(optimizer, 'dictionary') and optimizer.dictionary:
                dict_path = os.path.join(dict_dir, f'{language}_dictionary.json')
                optimizer.save_dictionary(dict_path)
        
        self.translation_server = TranslationServer(self.server_port)
        
        if background:
            # Start server in background thread
            server_thread = threading.Thread(target=self.translation_server.start_server)
            server_thread.daemon = True
            server_thread.start()
            
            print(f"🚀 Translation server started at http://localhost:{self.server_port}")
            return server_thread
        else:
            # Start server in foreground
            self.translation_server.start_server()
    
    def translate_code(self, optimized_code: str, language: str) -> str:
        """Translate optimized code back to human-readable"""
        if language in self.optimizers:
            optimizer = self.optimizers[language]
            return optimizer.get_human_readable(optimized_code)
        else:
            return f"# No translator available for {language}\n{optimized_code}"
    
    def translate_error(self, error_message: str, language: str) -> str:
        """Translate error message"""
        if language in self.optimizers:
            optimizer = self.optimizers[language]
            return optimizer.translate_error(error_message)
        else:
            return error_message

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Universal Code Optimizer - Extreme Optimization')
    
    parser.add_argument('input', nargs='?', help='Input file or directory to optimize')
    parser.add_argument('-o', '--output', help='Output file or directory')
    parser.add_argument('-s', '--strategy', 
                       choices=['memory_optimized', 'speed_optimized', 'hybrid'],
                       default='memory_optimized',
                       help='Optimization strategy')
    parser.add_argument('-r', '--recursive', action='store_true',
                       help='Process directories recursively')
    parser.add_argument('--server-port', type=int, default=8080,
                       help='Translation server port')
    parser.add_argument('--start-server', action='store_true',
                       help='Start translation server after optimization')
    parser.add_argument('--server-only', action='store_true',
                       help='Only start translation server (no optimization)')
    parser.add_argument('--extensions', nargs='+',
                       help='File extensions to process (e.g., .c .h .cpp)')
    
    args = parser.parse_args()
    
    # Initialize optimizer
    optimizer = UniversalOptimizer(args.strategy, args.server_port)
    
    if args.server_only:
        # Only start server
        print("🌐 Server-only mode")
        optimizer.start_translation_server(background=False)
        return
    
    if not args.input:
        parser.error("Input file or directory is required unless using --server-only")
    
    # Determine output path
    if args.output:
        output_path = args.output
    else:
        if os.path.isfile(args.input):
            name, ext = os.path.splitext(args.input)
            output_path = f"{name}.optimized{ext}"
        else:
            output_path = f"{args.input}.optimized"
    
    # Optimize
    if os.path.isfile(args.input):
        # Single file
        result = optimizer.optimize_file(args.input, output_path)
        
        if 'error' not in result:
            print(f"\n✅ Optimization complete!")
            print(f"Original: {result['original_size']:,} bytes")
            print(f"Optimized: {result['optimized_size']:,} bytes")
            print(f"Reduction: {result['reduction_percent']:.1f}%")
            print(f"Output: {result['output_path']}")
        
    else:
        # Directory
        results = optimizer.optimize_directory(
            args.input, 
            output_path, 
            args.recursive, 
            args.extensions
        )
    
    # Start translation server if requested
    if args.start_server:
        print(f"\n🌐 Starting translation server...")
        server_thread = optimizer.start_translation_server(background=True)
        
        try:
            print(f"🌐 Translation server running at http://localhost:{args.server_port}")
            print("Press Ctrl+C to stop...")
            
            while True:
                time.sleep(1)
                
        except KeyboardInterrupt:
            print("\n👋 Shutting down...")

if __name__ == "__main__":
    main()
