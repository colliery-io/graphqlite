// Version selector for mdBook
// Reads versions.json and creates a dropdown in the menu bar

(function() {
    'use strict';

    // Get current version from URL path
    function getCurrentVersion() {
        const path = window.location.pathname;
        const match = path.match(/^\/(v[\d.]+|latest)\//);
        return match ? match[1] : 'latest';
    }

    // Fetch versions and create dropdown
    async function initVersionSelect() {
        try {
            const response = await fetch('/versions.json');
            if (!response.ok) return;

            const versions = await response.json();
            if (!versions || versions.length === 0) return;

            const currentVersion = getCurrentVersion();

            // Create dropdown HTML
            const dropdown = document.createElement('div');
            dropdown.className = 'version-select';
            dropdown.innerHTML = `
                <label for="version-selector">Version: </label>
                <select id="version-selector">
                    ${versions.map(v => `<option value="${v}" ${v === currentVersion ? 'selected' : ''}>${v}</option>`).join('')}
                </select>
            `;

            // Add change handler
            dropdown.querySelector('select').addEventListener('change', function(e) {
                const newVersion = e.target.value;
                const currentPath = window.location.pathname;
                const newPath = currentPath.replace(/^\/(v[\d.]+|latest)\//, `/${newVersion}/`);
                window.location.href = newPath;
            });

            // Insert into menu bar
            const menuBar = document.querySelector('.left-buttons');
            if (menuBar) {
                menuBar.appendChild(dropdown);
            }
        } catch (e) {
            console.log('Version selector not available:', e.message);
        }
    }

    // Run when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initVersionSelect);
    } else {
        initVersionSelect();
    }
})();
