import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 20,
  duration: '30s',
};

export default function () {
  const res = http.post('http://127.0.0.1:8080/api/v1/functions/hello/invoke',
    JSON.stringify({ name: 'k6' }),
    { headers: { 'Content-Type': 'application/json' } });
  check(res, { 'status is 200': (r) => r.status === 200 });
  sleep(0.01);
}
