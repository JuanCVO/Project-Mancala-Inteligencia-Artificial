/**
 * Mancala Frontend Application
 * Handles UI interactions and communication with FastAPI backend
 */

// Configuration
const API_BASE = `http://${window.location.hostname}:8000`;
const TIMEOUT = 30000; // 30 seconds

// State
const state = {
    isLoading: false,
    lastResult: null,
};

// DOM Elements
const elements = {
    boardInput: document.getElementById('boardInput'),
    sideSelect: document.getElementById('sideSelect'),
    algoSelect: document.getElementById('algoSelect'),
    depthInput: document.getElementById('depthInput'),
    threadsInput: document.getElementById('threadsInput'),
    simulationsInput: document.getElementById('simulationsInput'),
    threadsInputMcts: document.getElementById('threadsInputMcts'),
    calculateButton: document.getElementById('calculateButton'),
    alphabetaParams: document.getElementById('alphabetaParams'),
    mctsParams: document.getElementById('mctsParams'),
    loadingSpinner: document.getElementById('loadingSpinner'),
    resultsContent: document.getElementById('resultsContent'),
    errorMessage: document.getElementById('errorMessage'),
    successMessage: document.getElementById('successMessage'),
    moveResult: document.getElementById('moveResult'),
    evaluationResult: document.getElementById('evaluationResult'),
    elapsedResult: document.getElementById('elapsedResult'),
    threadsResult: document.getElementById('threadsResult'),
    statsContent: document.getElementById('statsContent'),
    backendStatus: document.getElementById('backendStatus'),
    motorStatus: document.getElementById('motorStatus'),
    refreshStatusButton: document.getElementById('refreshStatusButton'),
};

/**
 * Initialize the application
 */
function initialize() {
    console.log('Initializing Mancala frontend...');
    setupEventListeners();
    checkServiceStatus();
    // Auto-check status every 10 seconds
    setInterval(checkServiceStatus, 10000);
}

/**
 * Setup event listeners
 */
function setupEventListeners() {
    elements.algoSelect.addEventListener('change', handleAlgorithmChange);
    elements.calculateButton.addEventListener('click', handleCalculateMove);
    elements.refreshStatusButton.addEventListener('click', checkServiceStatus);
}

/**
 * Handle algorithm selection change
 */
function handleAlgorithmChange() {
    const algo = elements.algoSelect.value;
    if (algo === 'alphabeta') {
        elements.alphabetaParams.classList.remove('hidden');
        elements.mctsParams.classList.add('hidden');
    } else {
        elements.alphabetaParams.classList.add('hidden');
        elements.mctsParams.classList.remove('hidden');
    }
}

/**
 * Parse board from input
 */
function parseBoard() {
    const input = elements.boardInput.value.trim();
    try {
        const board = JSON.parse(input);
        if (!Array.isArray(board) || board.length !== 14) {
            throw new Error('Board must be an array of 14 integers');
        }
        return board;
    } catch (error) {
        showError(`Invalid board format: ${error.message}`);
        return null;
    }
}

/**
 * Get request payload from form
 */
function getRequestPayload() {
    const board = parseBoard();
    if (!board) return null;

    const side = parseInt(elements.sideSelect.value, 10);
    const algo = elements.algoSelect.value;
    const threads = algo === 'alphabeta'
        ? parseInt(elements.threadsInput.value, 10)
        : parseInt(elements.threadsInputMcts.value, 10);

    const payload = {
        board,
        side,
        algo,
        threads,
    };

    if (algo === 'alphabeta') {
        payload.depth = parseInt(elements.depthInput.value, 10);
    } else {
        payload.simulations = parseInt(elements.simulationsInput.value, 10);
    }

    return payload;
}

/**
 * Handle calculate move button click
 */
async function handleCalculateMove() {
    if (state.isLoading) return;

    const payload = getRequestPayload();
    if (!payload) return;

    setLoading(true);
    clearMessages();

    try {
        const response = await fetchWithTimeout(`${API_BASE}/move`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
            },
            body: JSON.stringify(payload),
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.detail || `HTTP ${response.status}`);
        }

        const result = await response.json();
        state.lastResult = result;
        displayResult(result);
        showSuccess('Move calculated successfully!');
    } catch (error) {
        console.error('Error calculating move:', error);
        showError(`Failed to calculate move: ${error.message}`);
    } finally {
        setLoading(false);
    }
}

/**
 * Display result on the page
 */
function displayResult(result) {
    elements.moveResult.textContent = result.move >= 0 ? result.move : 'N/A';
    elements.evaluationResult.textContent = result.evaluation.toFixed(4);
    elements.elapsedResult.textContent = `${result.elapsed_ms} ms`;
    elements.threadsResult.textContent = result.threads_used;

    // Display stats
    const statsHtml = formatStats(result.stats);
    elements.statsContent.innerHTML = statsHtml;

    // Show results
    elements.resultsContent.classList.remove('hidden');
    elements.loadingSpinner.classList.add('hidden');
}

/**
 * Format statistics object as HTML
 */
function formatStats(stats) {
    if (!stats || Object.keys(stats).length === 0) {
        return '<p>No statistics available</p>';
    }

    let html = '';
    for (const [key, value] of Object.entries(stats)) {
        if (value !== null && value !== undefined) {
            const displayValue = typeof value === 'number' && !Number.isInteger(value)
                ? value.toFixed(2)
                : value;
            html += `
                <div class="stat-item">
                    <span class="stat-name">${formatStatName(key)}:</span>
                    <span class="stat-value">${displayValue}</span>
                </div>
            `;
        }
    }
    return html;
}

/**
 * Format statistic name for display
 */
function formatStatName(name) {
    const map = {
        'algo': 'Algorithm',
        'nodes': 'Nodes Explored',
        'prunes': 'Alpha-Beta Prunes',
        'rollouts': 'Rollouts',
        'tree_depth_avg': 'Avg Tree Depth',
        'win_rate': 'Win Rate',
    };
    return map[name] || name.replace(/_/g, ' ').toUpperCase();
}

/**
 * Set loading state
 */
function setLoading(loading) {
    state.isLoading = loading;
    elements.calculateButton.disabled = loading;
    if (loading) {
        elements.loadingSpinner.classList.remove('hidden');
        elements.resultsContent.classList.add('hidden');
    } else {
        elements.loadingSpinner.classList.add('hidden');
    }
}

/**
 * Show error message
 */
function showError(message) {
    elements.errorMessage.textContent = message;
    elements.errorMessage.classList.remove('hidden');
    elements.successMessage.classList.add('hidden');
}

/**
 * Show success message
 */
function showSuccess(message) {
    elements.successMessage.textContent = message;
    elements.successMessage.classList.remove('hidden');
    elements.errorMessage.classList.add('hidden');
}

/**
 * Clear messages
 */
function clearMessages() {
    elements.errorMessage.classList.add('hidden');
    elements.successMessage.classList.add('hidden');
}

/**
 * Check service status
 */
async function checkServiceStatus() {
    try {
        // Check backend health
        const healthResponse = await fetchWithTimeout(`${API_BASE}/healthz`, {
            method: 'GET',
        });
        updateStatusIndicator(
            elements.backendStatus,
            healthResponse.ok ? 'ok' : 'error'
        );

        // Check motor readiness
        const readyResponse = await fetchWithTimeout(`${API_BASE}/readyz`, {
            method: 'GET',
        });
        updateStatusIndicator(
            elements.motorStatus,
            readyResponse.ok ? 'ok' : 'error'
        );
    } catch (error) {
        console.error('Status check error:', error);
        updateStatusIndicator(elements.backendStatus, 'error');
        updateStatusIndicator(elements.motorStatus, 'error');
    }
}

/**
 * Update status indicator
 */
function updateStatusIndicator(element, status) {
    element.classList.remove('ok', 'error', 'pending');
    if (status === 'ok') {
        element.textContent = '✅';
        element.classList.add('ok');
    } else {
        element.textContent = '❌';
        element.classList.add('error');
    }
}

/**
 * Fetch with timeout
 */
function fetchWithTimeout(url, options = {}, timeout = TIMEOUT) {
    return Promise.race([
        fetch(url, options),
        new Promise((_, reject) =>
            setTimeout(() => reject(new Error('Request timeout')), timeout)
        ),
    ]);
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initialize);
} else {
    initialize();
}

