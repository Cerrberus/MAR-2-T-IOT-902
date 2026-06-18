import { BrowserRouter, Routes, Route, Link } from 'react-router-dom'
import Header from './components/Header'
import Dashboard from './pages/Dashboard'
import DeviceDetail from './pages/DeviceDetail'
import SensorCommunity from './pages/SensorCommunity'

function NotFound() {
  return (
    <div className="not-found">
      <div className="not-found-code">404</div>
      <p className="not-found-msg">Cette page n'existe pas.</p>
      <Link to="/" className="not-found-link">← Retour au tableau de bord</Link>
    </div>
  )
}

export default function App() {
  return (
    <BrowserRouter>
      <Header />
      <main className="main-content">
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/devices/:id" element={<DeviceDetail />} />
          <Route path="/community" element={<SensorCommunity />} />
          <Route path="*" element={<NotFound />} />
        </Routes>
      </main>
    </BrowserRouter>
  )
}
