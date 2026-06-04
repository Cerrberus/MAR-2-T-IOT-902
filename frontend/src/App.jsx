import { BrowserRouter, Routes, Route } from 'react-router-dom'
import Header from './components/Header'
import Dashboard from './pages/Dashboard'
import DeviceDetail from './pages/DeviceDetail'
import SensorCommunity from './pages/SensorCommunity'

export default function App() {
  return (
    <BrowserRouter>
      <Header />
      <main className="main-content">
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/devices/:id" element={<DeviceDetail />} />
          <Route path="/community" element={<SensorCommunity />} />
        </Routes>
      </main>
    </BrowserRouter>
  )
}
