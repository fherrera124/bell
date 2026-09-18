"""Local TLS termination tests; requires Python and the openssl CLI."""
import pathlib
import socket
import ssl
import subprocess
import sys
import tempfile
import threading


def run_case(probe, context, payload, orderly, expected):
    errors = []
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(5)
        port = listener.getsockname()[1]

        def serve():
            try:
                tcp, _ = listener.accept()
                tcp.settimeout(3)
                with context.wrap_socket(tcp, server_side=True) as tls:
                    if payload:
                        tls.sendall(payload)
                    if orderly:
                        try:
                            tls.unwrap().close()
                        except (ssl.SSLError, ConnectionError):
                            # A rejected or incomplete HTTP response can close immediately.
                            pass
                    else:
                        socket.socket(fileno=tls.detach()).close()
            except Exception as exc:
                errors.append(exc)

        worker = threading.Thread(target=serve)
        worker.start()
        result = subprocess.run([probe, str(port), expected], timeout=8,
                                capture_output=True, text=True)
        worker.join(timeout=5)
        assert not worker.is_alive(), "TLS server did not terminate"
        assert not errors, errors
        assert result.returncode == 0, result.stdout + result.stderr


def main():
    probe, openssl = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="bell-tls-test-") as directory:
        cert = pathlib.Path(directory) / "cert.pem"
        key = pathlib.Path(directory) / "key.pem"
        subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", str(key), "-out", str(cert), "-days", "1",
                        "-subj", "/CN=localhost"], check=True, capture_output=True)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.maximum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(cert, key)
        cases = [
            (b"", "eof"),
            (b"HTTP/1.1 200", "incomplete"),
            (b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nabc", "incomplete"),
            (b"HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc", "success"),
            (b"HTTP/1.1 200 OK\r\n\r\nabc", "success"),
        ]
        for orderly in (True, False):
            for payload, clean_expected in cases:
                complete = b"Content-Length: 3\r\n" in payload
                expected = clean_expected if orderly or complete else "tls"
                run_case(probe, context, payload, orderly, expected)
                print(f"TLS orderly={orderly}, bytes={len(payload)}, expected={expected}: passed")


if __name__ == "__main__":
    main()
