#!/usr/bin/env python3
"""
Translation Server for Human-Readable Code Viewing
Provides web interface for translating optimized code back to human-readable form
"""

import os
import json
import asyncio
from typing import Dict, List, Optional
from flask import Flask, request, jsonify, render_template_string, send_from_directory
from flask_cors import CORS
import threading
import time

class TranslationServer:
    """Web server for code translation and debugging"""
    
    def __init__(self, port: int = 8080):
        self.app = Flask(__name__)
        CORS(self.app)
        self.port = port
        self.dictionaries = {}  # language -> dictionary mapping
        self.active_projects = {}  # project_id -> project_info
        
        self._setup_routes()
        self._load_dictionaries()
    
    def _setup_routes(self):
        """Setup Flask routes"""
        
        @self.app.route('/')
        def index():
            """Main interface"""
            return render_template_string(self._get_html_template())
        
        @self.app.route('/api/translate', methods=['POST'])
        def translate_code():
            """Translate optimized code to human-readable"""
            data = request.get_json()
            
            code = data.get('code', '')
            language = data.get('language', 'c')
            project_id = data.get('project_id', 'default')
            
            try:
                translated = self._translate_code(code, language, project_id)
                return jsonify({
                    'success': True,
                    'original': code,
                    'translated': translated,
                    'language': language
                })
            except Exception as e:
                return jsonify({
                    'success': False,
                    'error': str(e)
                }), 400
        
        @self.app.route('/api/translate-error', methods=['POST'])
        def translate_error():
            """Translate error message"""
            data = request.get_json()
            
            error_message = data.get('error', '')
            language = data.get('language', 'c')
            project_id = data.get('project_id', 'default')
            
            try:
                translated = self._translate_error_message(error_message, language, project_id)
                suggestions = self._get_error_suggestions(error_message, translated)
                
                return jsonify({
                    'success': True,
                    'original_error': error_message,
                    'translated_error': translated,
                    'suggestions': suggestions,
                    'language': language
                })
            except Exception as e:
                return jsonify({
                    'success': False,
                    'error': str(e)
                }), 400
        
        @self.app.route('/api/projects', methods=['GET'])
        def list_projects():
            """List available projects"""
            return jsonify({
                'projects': list(self.active_projects.keys()),
                'dictionaries': list(self.dictionaries.keys())
            })
        
        @self.app.route('/api/dictionary/<language>', methods=['GET'])
        def get_dictionary(language):
            """Get dictionary for language"""
            if language in self.dictionaries:
                return jsonify({
                    'language': language,
                    'dictionary': self.dictionaries[language],
                    'count': len(self.dictionaries[language].get('forward_map', {}))
                })
            else:
                return jsonify({'error': 'Dictionary not found'}), 404
        
        @self.app.route('/api/upload-dictionary', methods=['POST'])
        def upload_dictionary():
            """Upload new dictionary"""
            if 'dictionary' not in request.files:
                return jsonify({'error': 'No file provided'}), 400
            
            file = request.files['dictionary']
            language = request.form.get('language', 'unknown')
            
            try:
                content = json.loads(file.read().decode('utf-8'))
                self.dictionaries[language] = content
                
                return jsonify({
                    'success': True,
                    'language': language,
                    'entries': len(content.get('forward_map', {}))
                })
            except Exception as e:
                return jsonify({'error': str(e)}), 400
        
        @self.app.route('/api/analyze-file', methods=['POST'])
        def analyze_file():
            """Analyze optimized file"""
            data = request.get_json()
            
            file_content = data.get('content', '')
            language = data.get('language', 'c')
            
            analysis = self._analyze_optimized_file(file_content, language)
            
            return jsonify({
                'success': True,
                'analysis': analysis
            })
    
    def _load_dictionaries(self):
        """Load available dictionaries"""
        dict_dir = os.path.join(os.path.dirname(__file__), '..', 'dictionaries')
        
        if os.path.exists(dict_dir):
            for filename in os.listdir(dict_dir):
                if filename.endswith('.json'):
                    try:
                        filepath = os.path.join(dict_dir, filename)
                        with open(filepath, 'r', encoding='utf-8') as f:
                            data = json.load(f)
                        
                        language = data.get('language', filename.replace('.json', ''))
                        self.dictionaries[language] = data
                        
                        print(f"📚 Loaded dictionary for {language}: {len(data.get('forward_map', {}))} entries")
                        
                    except Exception as e:
                        print(f"❌ Error loading {filename}: {e}")
    
    def _translate_code(self, optimized_code: str, language: str, project_id: str) -> str:
        """Translate optimized code to human-readable"""
        if language not in self.dictionaries:
            return f"# Dictionary for {language} not found\n{optimized_code}"
        
        dictionary = self.dictionaries[language]
        reverse_map = dictionary.get('reverse_map', {})
        
        translated = optimized_code
        
        # Sort by length (longer first to avoid partial replacements)
        sorted_symbols = sorted(reverse_map.keys(), key=len, reverse=True)
        
        for optimized_symbol in sorted_symbols:
            original = reverse_map[optimized_symbol]
            # Use word boundaries for identifiers, direct replacement for others
            if optimized_symbol.isalpha():
                import re
                translated = re.sub(r'\b' + re.escape(optimized_symbol) + r'\b', original, translated)
            else:
                translated = translated.replace(optimized_symbol, original)
        
        return translated
    
    def _translate_error_message(self, error_message: str, language: str, project_id: str) -> str:
        """Translate error message"""
        if language not in self.dictionaries:
            return error_message
        
        dictionary = self.dictionaries[language]
        reverse_map = dictionary.get('reverse_map', {})
        
        translated = error_message
        
        for optimized_symbol in reverse_map:
            original = reverse_map[optimized_symbol]
            translated = translated.replace(optimized_symbol, original)
        
        return translated
    
    def _get_error_suggestions(self, original_error: str, translated_error: str) -> List[str]:
        """Get suggestions for common errors"""
        suggestions = []
        
        # Common C/C++ error patterns
        if "undefined" in translated_error.lower():
            suggestions.append("Check if the identifier is properly declared")
            suggestions.append("Verify include statements for required headers")
            suggestions.append("Check for typos in identifier names")
        
        if "redefinition" in translated_error.lower():
            suggestions.append("Check for duplicate function/variable definitions")
            suggestions.append("Verify header guards are properly implemented")
        
        if "syntax error" in translated_error.lower():
            suggestions.append("Check for missing semicolons or braces")
            suggestions.append("Verify parentheses are properly balanced")
        
        if "type" in translated_error.lower():
            suggestions.append("Check variable types match function parameters")
            suggestions.append("Verify casting is done correctly")
        
        return suggestions
    
    def _analyze_optimized_file(self, content: str, language: str) -> Dict:
        """Analyze optimized file content"""
        analysis = {
            'total_lines': len(content.split('\n')),
            'total_characters': len(content),
            'optimized_symbols': 0,
            'optimization_ratio': 0.0,
            'symbol_breakdown': {},
            'readability_score': 0.0
        }
        
        if language in self.dictionaries:
            reverse_map = self.dictionaries[language].get('reverse_map', {})
            
            # Count optimized symbols
            for symbol in reverse_map.keys():
                count = content.count(symbol)
                if count > 0:
                    analysis['optimized_symbols'] += count
                    analysis['symbol_breakdown'][symbol] = count
            
            # Calculate optimization ratio
            total_symbols = len(content.split())
            if total_symbols > 0:
                analysis['optimization_ratio'] = (analysis['optimized_symbols'] / total_symbols) * 100
            
            # Calculate readability score (inverse of optimization ratio)
            analysis['readability_score'] = max(0, 100 - analysis['optimization_ratio'])
        
        return analysis
    
    def _get_html_template(self) -> str:
        """Get HTML template for web interface"""
        return '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Universal Code Translation Server</title>
    <style>
        body { font-family: 'Monaco', 'Consolas', monospace; margin: 0; padding: 20px; background: #1e1e1e; color: #d4d4d4; }
        .container { max-width: 1400px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 30px; }
        .header h1 { color: #4fc3f7; margin: 0; }
        .header p { color: #888; margin: 5px 0; }
        .tabs { display: flex; margin-bottom: 20px; border-bottom: 2px solid #333; }
        .tab { padding: 10px 20px; background: #2d2d2d; border: none; color: #d4d4d4; cursor: pointer; margin-right: 2px; }
        .tab.active { background: #4fc3f7; color: #1e1e1e; }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .input-group { margin-bottom: 15px; }
        .input-group label { display: block; margin-bottom: 5px; color: #4fc3f7; font-weight: bold; }
        .input-group select, .input-group input { width: 100%; padding: 8px; background: #2d2d2d; border: 1px solid #555; color: #d4d4d4; }
        .textarea-container { display: flex; gap: 20px; margin-bottom: 20px; }
        .textarea-wrapper { flex: 1; }
        .textarea-wrapper textarea { width: 100%; height: 300px; background: #2d2d2d; border: 1px solid #555; color: #d4d4d4; padding: 10px; font-family: inherit; resize: vertical; }
        .button { background: #4fc3f7; color: #1e1e1e; border: none; padding: 10px 20px; cursor: pointer; font-weight: bold; margin-right: 10px; }
        .button:hover { background: #29b6f6; }
        .button.secondary { background: #666; color: #fff; }
        .result { margin-top: 20px; padding: 15px; background: #2d2d2d; border-left: 4px solid #4fc3f7; }
        .error { border-left-color: #f44336; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }
        .stat-card { background: #2d2d2d; padding: 15px; border-radius: 5px; text-align: center; }
        .stat-card .value { font-size: 24px; font-weight: bold; color: #4fc3f7; }
        .stat-card .label { color: #888; margin-top: 5px; }
        .dictionary-list { max-height: 400px; overflow-y: auto; background: #2d2d2d; padding: 15px; }
        .dictionary-item { display: flex; justify-content: space-between; padding: 5px 0; border-bottom: 1px solid #444; }
        .suggestions { background: #2d2d2d; padding: 15px; margin-top: 10px; }
        .suggestions ul { margin: 10px 0; padding-left: 20px; }
        .suggestions li { margin: 5px 0; color: #81c784; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔤 Universal Code Translation Server</h1>
            <p>Translate optimized code back to human-readable form</p>
            <p><strong>Server Status:</strong> <span id="status">🟢 Online</span></p>
        </div>

        <div class="tabs">
            <button class="tab active" onclick="showTab('translate')">Code Translation</button>
            <button class="tab" onclick="showTab('error')">Error Translation</button>
            <button class="tab" onclick="showTab('analyze')">File Analysis</button>
            <button class="tab" onclick="showTab('dictionary')">Dictionaries</button>
        </div>

        <div id="translate" class="tab-content active">
            <h2>Code Translation</h2>
            <div class="input-group">
                <label>Language:</label>
                <select id="translateLanguage">
                    <option value="c">C</option>
                    <option value="cpp">C++</option>
                    <option value="javascript">JavaScript</option>
                    <option value="python">Python</option>
                </select>
            </div>
            <div class="textarea-container">
                <div class="textarea-wrapper">
                    <label>Optimized Code:</label>
                    <textarea id="optimizedCode" placeholder="Paste your optimized code here..."></textarea>
                </div>
                <div class="textarea-wrapper">
                    <label>Human-Readable Translation:</label>
                    <textarea id="translatedCode" placeholder="Translation will appear here..." readonly></textarea>
                </div>
            </div>
            <button class="button" onclick="translateCode()">🔄 Translate Code</button>
            <button class="button secondary" onclick="clearTranslation()">🗑️ Clear</button>
        </div>

        <div id="error" class="tab-content">
            <h2>Error Message Translation</h2>
            <div class="input-group">
                <label>Language:</label>
                <select id="errorLanguage">
                    <option value="c">C</option>
                    <option value="cpp">C++</option>
                    <option value="javascript">JavaScript</option>
                    <option value="python">Python</option>
                </select>
            </div>
            <div class="input-group">
                <label>Error Message:</label>
                <textarea id="errorMessage" rows="4" placeholder="Paste error message here..."></textarea>
            </div>
            <button class="button" onclick="translateError()">🔄 Translate Error</button>
            <div id="errorResult"></div>
        </div>

        <div id="analyze" class="tab-content">
            <h2>File Analysis</h2>
            <div class="input-group">
                <label>Language:</label>
                <select id="analyzeLanguage">
                    <option value="c">C</option>
                    <option value="cpp">C++</option>
                    <option value="javascript">JavaScript</option>
                    <option value="python">Python</option>
                </select>
            </div>
            <div class="input-group">
                <label>File Content:</label>
                <textarea id="fileContent" rows="10" placeholder="Paste file content here..."></textarea>
            </div>
            <button class="button" onclick="analyzeFile()">📊 Analyze File</button>
            <div id="analysisResult"></div>
        </div>

        <div id="dictionary" class="tab-content">
            <h2>Available Dictionaries</h2>
            <button class="button" onclick="loadDictionaries()">🔄 Refresh Dictionaries</button>
            <div id="dictionaryList" class="dictionary-list">
                <p>Loading dictionaries...</p>
            </div>
        </div>
    </div>

    <script>
        function showTab(tabName) {
            // Hide all tab contents
            const contents = document.querySelectorAll('.tab-content');
            contents.forEach(content => content.classList.remove('active'));
            
            // Remove active class from all tabs
            const tabs = document.querySelectorAll('.tab');
            tabs.forEach(tab => tab.classList.remove('active'));
            
            // Show selected tab content and mark tab as active
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');
        }

        async function translateCode() {
            const code = document.getElementById('optimizedCode').value;
            const language = document.getElementById('translateLanguage').value;
            
            if (!code.trim()) {
                alert('Please enter code to translate');
                return;
            }
            
            try {
                const response = await fetch('/api/translate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ code, language })
                });
                
                const result = await response.json();
                
                if (result.success) {
                    document.getElementById('translatedCode').value = result.translated;
                } else {
                    alert('Translation failed: ' + result.error);
                }
            } catch (error) {
                alert('Network error: ' + error.message);
            }
        }

        async function translateError() {
            const error = document.getElementById('errorMessage').value;
            const language = document.getElementById('errorLanguage').value;
            
            if (!error.trim()) {
                alert('Please enter an error message');
                return;
            }
            
            try {
                const response = await fetch('/api/translate-error', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ error, language })
                });
                
                const result = await response.json();
                
                const resultDiv = document.getElementById('errorResult');
                
                if (result.success) {
                    resultDiv.innerHTML = `
                        <div class="result">
                            <h3>🔍 Translated Error:</h3>
                            <p><strong>Original:</strong> ${result.original_error}</p>
                            <p><strong>Translated:</strong> ${result.translated_error}</p>
                            ${result.suggestions.length > 0 ? `
                                <div class="suggestions">
                                    <h4>💡 Suggestions:</h4>
                                    <ul>
                                        ${result.suggestions.map(s => `<li>${s}</li>`).join('')}
                                    </ul>
                                </div>
                            ` : ''}
                        </div>
                    `;
                } else {
                    resultDiv.innerHTML = `<div class="result error">Error: ${result.error}</div>`;
                }
            } catch (error) {
                document.getElementById('errorResult').innerHTML = 
                    `<div class="result error">Network error: ${error.message}</div>`;
            }
        }

        async function analyzeFile() {
            const content = document.getElementById('fileContent').value;
            const language = document.getElementById('analyzeLanguage').value;
            
            if (!content.trim()) {
                alert('Please enter file content');
                return;
            }
            
            try {
                const response = await fetch('/api/analyze-file', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ content, language })
                });
                
                const result = await response.json();
                
                if (result.success) {
                    const analysis = result.analysis;
                    document.getElementById('analysisResult').innerHTML = `
                        <div class="stats">
                            <div class="stat-card">
                                <div class="value">${analysis.total_lines}</div>
                                <div class="label">Lines</div>
                            </div>
                            <div class="stat-card">
                                <div class="value">${analysis.total_characters}</div>
                                <div class="label">Characters</div>
                            </div>
                            <div class="stat-card">
                                <div class="value">${analysis.optimized_symbols}</div>
                                <div class="label">Optimized Symbols</div>
                            </div>
                            <div class="stat-card">
                                <div class="value">${analysis.optimization_ratio.toFixed(1)}%</div>
                                <div class="label">Optimization Ratio</div>
                            </div>
                            <div class="stat-card">
                                <div class="value">${analysis.readability_score.toFixed(1)}%</div>
                                <div class="label">Readability Score</div>
                            </div>
                        </div>
                    `;
                } else {
                    document.getElementById('analysisResult').innerHTML = 
                        `<div class="result error">Analysis failed: ${result.error}</div>`;
                }
            } catch (error) {
                document.getElementById('analysisResult').innerHTML = 
                    `<div class="result error">Network error: ${error.message}</div>`;
            }
        }

        async function loadDictionaries() {
            try {
                const response = await fetch('/api/projects');
                const result = await response.json();
                
                const listDiv = document.getElementById('dictionaryList');
                
                if (result.dictionaries && result.dictionaries.length > 0) {
                    listDiv.innerHTML = result.dictionaries.map(lang => `
                        <div class="dictionary-item">
                            <span><strong>${lang.toUpperCase()}</strong> Dictionary</span>
                            <button class="button" onclick="viewDictionary('${lang}')">View</button>
                        </div>
                    `).join('');
                } else {
                    listDiv.innerHTML = '<p>No dictionaries available</p>';
                }
            } catch (error) {
                document.getElementById('dictionaryList').innerHTML = 
                    `<p class="error">Error loading dictionaries: ${error.message}</p>`;
            }
        }

        async function viewDictionary(language) {
            try {
                const response = await fetch(`/api/dictionary/${language}`);
                const result = await response.json();
                
                if (result.dictionary) {
                    const entries = Object.entries(result.dictionary.forward_map || {}).slice(0, 20);
                    const preview = entries.map(([orig, opt]) => `${orig} → ${opt}`).join('\\n');
                    
                    alert(`${language.toUpperCase()} Dictionary Preview (first 20 entries):\\n\\n${preview}\\n\\n... and ${result.count - 20} more entries`);
                }
            } catch (error) {
                alert('Error viewing dictionary: ' + error.message);
            }
        }

        function clearTranslation() {
            document.getElementById('optimizedCode').value = '';
            document.getElementById('translatedCode').value = '';
        }

        // Load dictionaries on page load
        window.addEventListener('load', loadDictionaries);

        // Check server status
        setInterval(async () => {
            try {
                const response = await fetch('/api/projects');
                document.getElementById('status').innerHTML = '🟢 Online';
            } catch (error) {
                document.getElementById('status').innerHTML = '🔴 Offline';
            }
        }, 30000);
    </script>
</body>
</html>
        '''
    
    def start_server(self):
        """Start the translation server"""
        print(f"🚀 Starting Translation Server on port {self.port}")
        print(f"🌐 Access at: http://localhost:{self.port}")
        print(f"📚 Loaded {len(self.dictionaries)} dictionaries")
        
        self.app.run(host='0.0.0.0', port=self.port, debug=False, threaded=True)
    
    def add_project(self, project_id: str, project_info: Dict):
        """Add project to server"""
        self.active_projects[project_id] = project_info
        print(f"📁 Added project: {project_id}")

if __name__ == "__main__":
    # Start the translation server
    server = TranslationServer(port=8080)
    
    print("🔤 Universal Code Translation Server")
    print("=" * 50)
    
    try:
        server.start_server()
    except KeyboardInterrupt:
        print("\n👋 Server stopped by user")
