#!/usr/bin/env python3
"""
Universal Language Detector for Extreme Code Optimization
Detects programming language and routes to appropriate optimizer
"""

import os
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple

class LanguageDetector:
    """Detects programming language from file content and extension"""
    
    LANGUAGE_PATTERNS = {
        'c': {
            'extensions': ['.c', '.h'],
            'patterns': [
                r'#include\s*[<"]',
                r'\bstruct\s+\w+',
                r'\btypedef\s+',
                r'\bprintf\s*\(',
                r'void\s+main\s*\(',
                r'malloc\s*\(',
            ],
            'keywords': ['auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
                        'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
                        'int', 'long', 'register', 'return', 'short', 'signed', 'sizeof',
                        'static', 'struct', 'switch', 'typedef', 'union', 'unsigned',
                        'void', 'volatile', 'while']
        },
        
        'cpp': {
            'extensions': ['.cpp', '.cc', '.cxx', '.hpp', '.hxx', '.h++'],
            'patterns': [
                r'#include\s*[<"]',
                r'\bclass\s+\w+',
                r'\bnamespace\s+\w+',
                r'\bstd::\w+',
                r'\btemplate\s*<',
                r'\bpublic:|private:|protected:',
                r'cout\s*<<',
            ],
            'keywords': ['class', 'namespace', 'template', 'public', 'private', 'protected',
                        'virtual', 'override', 'final', 'constexpr', 'nullptr', 'auto',
                        'decltype', 'typename', 'using', 'operator', 'friend', 'inline',
                        'explicit', 'mutable', 'thread_local', 'alignas', 'alignof']
        },
        
        'javascript': {
            'extensions': ['.js', '.jsx', '.mjs', '.es6'],
            'patterns': [
                r'\bfunction\s+\w+',
                r'\bconst\s+\w+\s*=',
                r'\blet\s+\w+\s*=',
                r'\bvar\s+\w+\s*=',
                r'=>\s*{',
                r'require\s*\(',
                r'import\s+.*from',
                r'console\.log\s*\(',
            ],
            'keywords': ['function', 'var', 'let', 'const', 'if', 'else', 'for', 'while',
                        'do', 'switch', 'case', 'default', 'break', 'continue', 'return',
                        'try', 'catch', 'finally', 'throw', 'new', 'this', 'typeof',
                        'instanceof', 'in', 'of', 'class', 'extends', 'super', 'static',
                        'async', 'await', 'yield', 'import', 'export', 'from', 'as']
        },
        
        'python': {
            'extensions': ['.py', '.pyw', '.py3'],
            'patterns': [
                r'\bdef\s+\w+\s*\(',
                r'\bclass\s+\w+',
                r'\bimport\s+\w+',
                r'\bfrom\s+\w+\s+import',
                r'if\s+__name__\s*==',
                r'print\s*\(',
                r'#\s*!/usr/bin/env python',
            ],
            'keywords': ['and', 'as', 'assert', 'break', 'class', 'continue', 'def',
                        'del', 'elif', 'else', 'except', 'exec', 'finally', 'for',
                        'from', 'global', 'if', 'import', 'in', 'is', 'lambda',
                        'not', 'or', 'pass', 'print', 'raise', 'return', 'try',
                        'while', 'with', 'yield', 'async', 'await', 'nonlocal']
        },
        
        'rust': {
            'extensions': ['.rs'],
            'patterns': [
                r'\bfn\s+\w+',
                r'\bstruct\s+\w+',
                r'\bimpl\s+\w+',
                r'\bmatch\s+\w+',
                r'\buse\s+\w+',
                r'println!\s*\(',
                r'#\[derive\(',
            ],
            'keywords': ['as', 'break', 'const', 'continue', 'crate', 'else', 'enum',
                        'extern', 'false', 'fn', 'for', 'if', 'impl', 'in', 'let',
                        'loop', 'match', 'mod', 'move', 'mut', 'pub', 'ref',
                        'return', 'self', 'Self', 'static', 'struct', 'super',
                        'trait', 'true', 'type', 'unsafe', 'use', 'where', 'while']
        },
        
        'go': {
            'extensions': ['.go'],
            'patterns': [
                r'\bfunc\s+\w+',
                r'\bpackage\s+\w+',
                r'\bimport\s+\w+',
                r'\bstruct\s*{',
                r'\binterface\s*{',
                r'fmt\.Printf',
                r'go\s+func',
            ],
            'keywords': ['break', 'case', 'chan', 'const', 'continue', 'default',
                        'defer', 'else', 'fallthrough', 'for', 'func', 'go',
                        'goto', 'if', 'import', 'interface', 'map', 'package',
                        'range', 'return', 'select', 'struct', 'switch', 'type',
                        'var']
        }
    }
    
    def __init__(self):
        self.confidence_threshold = 0.3
    
    def detect_language(self, file_path: str, content: Optional[str] = None) -> Tuple[str, float]:
        """
        Detect programming language from file path and content
        Returns (language, confidence_score)
        """
        if content is None:
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
            except Exception:
                content = ""
        
        file_ext = Path(file_path).suffix.lower()
        scores = {}
        
        for lang, config in self.LANGUAGE_PATTERNS.items():
            score = 0.0
            
            # Extension matching (high weight)
            if file_ext in config['extensions']:
                score += 0.5
            
            # Pattern matching
            pattern_matches = 0
            for pattern in config['patterns']:
                if re.search(pattern, content, re.MULTILINE):
                    pattern_matches += 1
            
            if config['patterns']:
                score += (pattern_matches / len(config['patterns'])) * 0.3
            
            # Keyword density
            keyword_count = 0
            words = re.findall(r'\b\w+\b', content)
            total_words = len(words)
            
            if total_words > 0:
                for keyword in config['keywords']:
                    keyword_count += words.count(keyword)
                
                keyword_density = keyword_count / total_words
                score += min(keyword_density * 10, 0.2)  # Cap at 0.2
            
            scores[lang] = score
        
        # Find best match
        best_lang = max(scores, key=scores.get) if scores else 'unknown'
        best_score = scores.get(best_lang, 0.0)
        
        # Return unknown if confidence is too low
        if best_score < self.confidence_threshold:
            return 'unknown', best_score
        
        return best_lang, best_score
    
    def get_optimizer_class(self, language: str):
        """Get the appropriate optimizer class for detected language"""
        optimizers = {
            'c': 'COptimizer',
            'cpp': 'CppOptimizer', 
            'javascript': 'JavaScriptOptimizer',
            'python': 'PythonOptimizer',
            'rust': 'RustOptimizer',
            'go': 'GoOptimizer'
        }
        return optimizers.get(language, 'UnknownOptimizer')
    
    def analyze_directory(self, directory: str) -> Dict[str, List[str]]:
        """Analyze all files in directory and group by detected language"""
        results = {}
        
        for root, dirs, files in os.walk(directory):
            # Skip common non-source directories
            dirs[:] = [d for d in dirs if d not in ['.git', '.svn', 'node_modules', '__pycache__', 'build', 'dist']]
            
            for file in files:
                file_path = os.path.join(root, file)
                
                # Skip binary files and common non-source files
                if any(file.endswith(ext) for ext in ['.exe', '.dll', '.so', '.dylib', '.a', '.o', '.obj', '.bin']):
                    continue
                
                try:
                    language, confidence = self.detect_language(file_path)
                    
                    if language not in results:
                        results[language] = []
                    
                    results[language].append({
                        'path': file_path,
                        'confidence': confidence,
                        'size': os.path.getsize(file_path)
                    })
                    
                except Exception as e:
                    print(f"Error analyzing {file_path}: {e}")
        
        return results

if __name__ == "__main__":
    # Test the detector
    detector = LanguageDetector()
    
    # Test with zstd directory
    print("🔍 Analyzing zstd repository...")
    results = detector.analyze_directory("/Users/camilopineda/Desktop/sdcode/zstd")
    
    for language, files in results.items():
        print(f"\n📁 {language.upper()}: {len(files)} files")
        total_size = sum(f['size'] for f in files)
        print(f"   Total size: {total_size:,} bytes")
        
        # Show top 5 largest files
        top_files = sorted(files, key=lambda x: x['size'], reverse=True)[:5]
        for f in top_files:
            print(f"   - {f['path']} ({f['size']:,} bytes, confidence: {f['confidence']:.2f})")
