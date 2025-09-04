#!/bin/bash

# Universal Code Optimizer - GitHub Setup Script

echo "🚀 Universal Code Optimizer - GitHub Setup"
echo "=========================================="

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "❌ Not in a git repository. Please run 'git init' first."
    exit 1
fi

# Get repository name
REPO_NAME="universal-code-optimizer"

echo "📝 Repository: $REPO_NAME"
echo ""

# Instructions for GitHub setup
echo "🔧 GitHub Setup Instructions:"
echo "=============================="
echo ""
echo "1. Create a new repository on GitHub:"
echo "   - Go to: https://github.com/new"
echo "   - Repository name: $REPO_NAME"
echo "   - Description: 🚀 Extreme code optimization system - Remove ALL human legibility, achieve 95% size reduction"
echo "   - Make it Public"
echo "   - Don't initialize with README (we already have one)"
echo ""

echo "2. Copy and run these commands:"
echo "   git remote add origin https://github.com/YOUR_USERNAME/$REPO_NAME.git"
echo "   git branch -M main"
echo "   git push -u origin main"
echo ""

echo "3. Or if you want to push now, enter your GitHub username:"
read -p "GitHub username (or press Enter to skip): " GITHUB_USERNAME

if [ ! -z "$GITHUB_USERNAME" ]; then
    echo ""
    echo "🔗 Adding remote origin..."
    git remote add origin https://github.com/$GITHUB_USERNAME/$REPO_NAME.git
    
    echo "🌿 Renaming branch to main..."
    git branch -M main
    
    echo "📤 Pushing to GitHub..."
    git push -u origin main
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ Successfully pushed to GitHub!"
        echo "🌐 Repository URL: https://github.com/$GITHUB_USERNAME/$REPO_NAME"
        echo ""
        echo "🎯 Next steps:"
        echo "- Add repository description and topics"
        echo "- Enable GitHub Pages for documentation"
        echo "- Add collaborators if needed"
        echo "- Set up GitHub Actions for CI/CD"
    else
        echo "❌ Push failed. Please check your credentials and try again."
        echo "💡 You might need to:"
        echo "   - Set up SSH keys: https://docs.github.com/en/authentication/connecting-to-github-with-ssh"
        echo "   - Use personal access token: https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token"
    fi
else
    echo "⏭️  Skipped automatic push. Use the commands above when ready."
fi

echo ""
echo "🎉 Repository is ready for GitHub!"
echo ""
echo "📋 Repository Features:"
echo "- 🔥 94.5% code size reduction"
echo "- 🧠 Multi-language support"
echo "- 🌐 Interactive web showcase"
echo "- 📊 Real-time translation server"
echo "- 🛡️ Production-ready safety features"
echo "- 📖 Complete documentation"
echo ""
echo "🚀 Start showcasing: ./deploy_showcase.sh"
