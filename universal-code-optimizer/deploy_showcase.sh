#!/bin/bash

# Universal Code Optimizer - Showcase Deployment Script

echo "🚀 Universal Code Optimizer - Showcase Deployment"
echo "================================================="

# Check if virtual environment exists
if [ ! -d "venv" ]; then
    echo "📦 Creating virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source venv/bin/activate

# Install dependencies
echo "📥 Installing dependencies..."
pip install -r requirements.txt

# Check if Flask is installed
python -c "import flask" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "❌ Flask not installed properly"
    exit 1
fi

echo "✅ Dependencies installed successfully"

# Start showcase app
echo ""
echo "🎯 Starting showcase application..."
echo "🌐 Access at: http://localhost:5000"
echo "📱 Interactive demo with real-time optimization"
echo ""
echo "Press Ctrl+C to stop the server"
echo "================================================="

python showcase_app.py
