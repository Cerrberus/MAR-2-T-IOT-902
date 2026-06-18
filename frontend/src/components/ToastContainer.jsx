import PropTypes from 'prop-types'

export default function ToastContainer({ toasts }) {
  if (toasts.length === 0) return null
  return (
    <div className="toast-container">
      {toasts.map((t) => (
        <div key={t.id} className="toast">
          <span className="toast-dot" />
          {t.message}
        </div>
      ))}
    </div>
  )
}

ToastContainer.propTypes = {
  toasts: PropTypes.arrayOf(
    PropTypes.shape({ id: PropTypes.number.isRequired, message: PropTypes.string.isRequired }),
  ).isRequired,
}
