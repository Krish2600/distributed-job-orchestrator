import React, { useState, useEffect } from 'react';
import SubmitJob from './SubmitJob.jsx';
import Dashboard from './Dashboard.jsx';
import { Layers, Activity, RefreshCw } from 'lucide-react';

export default function App() {
  const [jobs, setJobs] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [pollingActive, setPollingActive] = useState(true);

  const fetchJobs = async () => {
    try {
      const response = await fetch('/api/jobs');
      if (!response.ok) {
        throw new Error('Failed to retrieve task list from backend server.');
      }
      const data = await response.json();
      setJobs(data);
      setError('');
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  // Perform initial fetch on load
  useEffect(() => {
    fetchJobs();
  }, []);

  // Poll database every 3 seconds for updates
  useEffect(() => {
    if (!pollingActive) return;
    const interval = setInterval(fetchJobs, 3000);
    return () => clearInterval(interval);
  }, [pollingActive]);

  const handleRetryJob = async (id) => {
    try {
      const response = await fetch(`/api/jobs/${id}/retry`, {
        method: 'POST',
      });
      if (!response.ok) {
        throw new Error('Retry command rejected by backend API');
      }
      // Instantly refresh after triggering a retry
      fetchJobs();
    } catch (err) {
      alert(`Retry failed: ${err.message}`);
    }
  };

  return (
    <div className="min-h-screen bg-[#080b11] text-slate-100 flex flex-col antialiased">
      {/* 1. Sleek Navigation Header */}
      <header className="sticky top-0 z-40 w-full bg-[#080b11]/80 backdrop-blur-md border-b border-slate-900 px-6 py-4 flex items-center justify-between">
        <div className="flex items-center space-x-3.5">
          <div className="p-2 bg-indigo-600 rounded-xl shadow-lg shadow-indigo-600/30 flex items-center justify-center animate-glow">
            <Layers className="w-5.5 h-5.5 text-white" />
          </div>
          <div>
            <h1 className="text-lg font-bold tracking-tight text-white flex items-center">
              Distributed Job Platform
              <span className="ml-2.5 px-2 py-0.5 text-[10px] font-semibold text-indigo-400 bg-indigo-500/10 border border-indigo-500/20 rounded-full uppercase tracking-wider">
                Vite + Crow + Postgres + Redis
              </span>
            </h1>
            <p className="text-[11px] text-slate-400 font-medium mt-0.5">High Performance Background Task Processing Queue</p>
          </div>
        </div>

        {/* Polling Indicator and Manual Refresh Action */}
        <div className="flex items-center space-x-4">
          <button
            onClick={() => setPollingActive(!pollingActive)}
            className="flex items-center space-x-2 text-xs font-semibold px-3.5 py-2 rounded-xl bg-[#1e293b]/50 border border-slate-800 hover:border-slate-700 transition-all cursor-pointer"
          >
            <Activity className={`w-3.5 h-3.5 ${pollingActive ? 'text-indigo-400 animate-pulse' : 'text-slate-500'}`} />
            <span className="text-slate-300">{pollingActive ? 'Auto-Polling Active' : 'Polling Suspended'}</span>
          </button>

          <button
            onClick={fetchJobs}
            disabled={loading}
            className="p-2.5 rounded-xl bg-[#1e293b]/50 border border-slate-800 hover:border-slate-700 text-indigo-400 hover:text-indigo-300 disabled:opacity-50 transition-all cursor-pointer"
            title="Manual sync"
          >
            <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          </button>
        </div>
      </header>

      {/* 2. Main Content Grid */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-4 md:p-6 lg:p-8">
        {error && (
          <div className="mb-6 p-4 bg-rose-500/10 border border-rose-500/20 rounded-2xl flex items-center space-x-3 text-sm text-rose-400 font-semibold animate-bounce">
            <span className="w-2 h-2 rounded-full bg-rose-500 animate-ping" />
            <span>Connection Warning: {error}</span>
          </div>
        )}

        <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 lg:gap-8 items-start">
          {/* Submit form: left column */}
          <div className="lg:col-span-4 sticky lg:top-24">
            <SubmitJob onJobSubmitted={fetchJobs} />
          </div>

          {/* Job dashboard list: right column */}
          <div className="lg:col-span-8">
            <Dashboard jobs={jobs} onRetryJob={handleRetryJob} />
          </div>
        </div>
      </main>

      {/* 3. Footer */}
      <footer className="w-full py-6 bg-[#080b11] border-t border-slate-900 text-center text-xs text-slate-500 font-medium mt-12">
        <p>Distributed Job Processing Platform. Demonstrating thread pools, message queues, and real-time state synchronization.</p>
      </footer>
    </div>
  );
}
