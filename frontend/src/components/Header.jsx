import { Link } from 'react-router-dom'

export default function Header() {
  return (
    <header className="app-header">
      <Link to="/" className="logo">
        🌿 Sensor Sensei
      </Link>
      <nav>
        <Link to="/">Tableau de bord</Link>
      </nav>
    </header>
  )
}
